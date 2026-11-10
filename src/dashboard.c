/*
 * src/dashboard.c
 * Member 3 -- ncurses Interactive Dashboard
 *
 * Implements the terminal UI for Process Guardian.
 *
 * Rules (MASTER_SPEC.md section 5.3 and MEMBER_3.md Phase A Step A4):
 *   - Use ncurses; initialise and restore terminal state correctly.
 *   - Display process table (PID, PPID, name, state, CPU%, memory, priority).
 *   - Display process tree view, recent anomaly alerts and a key-help line.
 *   - Allow row selection with up/down arrow keys.
 *   - Keys returned to main.c: q, t, s, c, k, p, l (and arrow keys).
 *     The dashboard handles its own view-state toggles (t, l) and selection
 *     (up/down) so that main.c only sees the final key code.
 *   - MUST NOT call kill(), setpriority(), getpriority() or logger functions.
 *   - Handle terminal resize and empty process lists without crashing.
 *   - Never display uninitialized array entries.
 */

#include "dashboard.h"

#include <ncurses.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

/* -------------------------------------------------------------------------
 * Module-level state
 * ------------------------------------------------------------------------- */

static int g_initialized    = 0;  /* 1 after a successful dashboard_init()  */
static int g_selected_index = 0;  /* Row in the currently displayed list    */
static int g_selected_pid   = -1; /* PID of the highlighted row; -1 = none  */
static int g_view_mode      = 0;  /* 0 = process list, 1 = tree view        */
static int g_log_view       = 0;  /* 0 = anomaly section, 1 = log message   */
static int g_scroll_top     = 0;  /* Index of the first visible list row    */

/* ncurses colour-pair identifiers */
#define CP_HEADER   1  /* Title bar and column headers */
#define CP_SELECTED 2  /* Highlighted/selected row     */
#define CP_SECTION  3  /* Section separator label      */
#define CP_ALERT    4  /* Anomaly alert text           */

/* Minimum terminal dimensions required to render the full layout */
#define MIN_COLS 40
#define MIN_ROWS  5

/* Number of rows reserved at the bottom for anomaly section and help line */
#define BOTTOM_RESERVE 5  /* 1 separator + 3 event rows + 1 help */

/* -------------------------------------------------------------------------
 * dashboard_init
 * ------------------------------------------------------------------------- */
int dashboard_init(void)
{
    if (g_initialized) {
        return 0; /* Already initialised; safe to call again. */
    }

    if (initscr() == NULL) {
        return -1;
    }

    cbreak();             /* Deliver keys immediately without buffering    */
    noecho();             /* Do not echo typed characters to the screen    */
    keypad(stdscr, TRUE); /* Translate arrow keys into KEY_UP / KEY_DOWN   */
    nodelay(stdscr, TRUE);/* Non-blocking getch() so the monitor loop runs */
    curs_set(0);          /* Hide the blinking cursor                       */

    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(CP_HEADER,   COLOR_BLACK,  COLOR_CYAN);
        init_pair(CP_SELECTED, COLOR_BLACK,  COLOR_GREEN);
        init_pair(CP_SECTION,  COLOR_YELLOW, COLOR_BLACK);
        init_pair(CP_ALERT,    COLOR_RED,    COLOR_BLACK);
    }

    g_initialized    = 1;
    g_selected_index = 0;
    g_selected_pid   = -1;
    g_view_mode      = 0;
    g_log_view       = 0;
    g_scroll_top     = 0;

    return 0;
}

/* -------------------------------------------------------------------------
 * dashboard_shutdown
 *
 * Restores normal terminal state.  Safe to call multiple times.
 * ------------------------------------------------------------------------- */
void dashboard_shutdown(void)
{
    if (!g_initialized) {
        return;
    }
    endwin();
    g_initialized = 0;
}

/* -------------------------------------------------------------------------
 * safe_print  (internal)
 *
 * Prints a formatted string at (row, col), truncating at max_col so we
 * never attempt to write past the terminal's right edge.
 * ------------------------------------------------------------------------- */
static void safe_print(int row, int col, int max_col, const char *fmt, ...)
{
    if (row < 0 || col < 0 || col >= max_col) {
        return;
    }

    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    /* Truncate so we do not write past the terminal's right margin. */
    int avail = max_col - col;
    if (avail <= 0) {
        return;
    }
    if (avail < (int)sizeof(buf)) {
        buf[avail] = '\0';
    }

    mvprintw(row, col, "%s", buf);
}

/* -------------------------------------------------------------------------
 * dashboard_render
 *
 * Redraws the entire screen with the supplied process and anomaly data.
 * Layout (top to bottom):
 *   Row 0          : title bar
 *   Row 1          : column headers
 *   Rows 2 .. N-5  : scrollable process list or tree
 *   Row N-4        : section separator
 *   Rows N-3 .. N-2: anomaly/log entries (up to 2 lines visible)
 *   Row N-1        : key-help bar
 * ------------------------------------------------------------------------- */
void dashboard_render(const ProcessInfo processes[],
                       int process_count,
                       const ProcessTreeNode nodes[],
                       int node_count,
                       const AnomalyEvent anomalies[],
                       int anomaly_count)
{
    if (!g_initialized) {
        return;
    }

    /* Clamp all counts to non-negative values. */
    if (process_count < 0) process_count = 0;
    if (node_count    < 0) node_count    = 0;
    if (anomaly_count < 0) anomaly_count = 0;

    /* How many items are in the currently active display list? */
    int display_count = (g_view_mode == 0) ? process_count : node_count;

    /* Keep selected_index inside [0, display_count - 1]. */
    if (display_count <= 0) {
        g_selected_index = 0;
        g_selected_pid   = -1;
    } else {
        if (g_selected_index >= display_count) {
            g_selected_index = display_count - 1;
        }
        if (g_selected_index < 0) {
            g_selected_index = 0;
        }
        g_selected_pid = (g_view_mode == 0)
            ? (int)processes[g_selected_index].pid
            : (int)nodes[g_selected_index].pid;
    }

    erase();

    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    /* Refuse to render in a terminal that is too small. */
    if (max_y < MIN_ROWS || max_x < MIN_COLS) {
        mvprintw(0, 0,
                 "Terminal too small. Resize to at least %dx%d.",
                 MIN_COLS, MIN_ROWS);
        refresh();
        return;
    }

    /* ------------------------------------------------------------------ */
    /* Row 0: Title bar                                                      */
    /* ------------------------------------------------------------------ */
    int row = 0;
    if (has_colors()) attron(COLOR_PAIR(CP_HEADER) | A_BOLD);
    mvhline(row, 0, ' ', max_x);
    const char *view_label = (g_view_mode == 0) ? "[List View]" : "[Tree View]";
    safe_print(row, 0, max_x,
               "  OS Process Guardian  %s  Processes: %d",
               view_label, process_count);
    if (has_colors()) attroff(COLOR_PAIR(CP_HEADER) | A_BOLD);
    row++;

    /* ------------------------------------------------------------------ */
    /* Row 1: Column headers                                                 */
    /* ------------------------------------------------------------------ */
    if (has_colors()) attron(COLOR_PAIR(CP_HEADER));
    mvhline(row, 0, ' ', max_x);
    if (g_view_mode == 0) {
        safe_print(row, 0, max_x,
                   " %-7s %-7s %-20s %-5s %7s %10s %5s",
                   "PID", "PPID", "NAME", "STATE", "CPU%", "MEM(KiB)", "NICE");
    } else {
        safe_print(row, 0, max_x,
                   " %-7s %-7s %-3s  %s",
                   "PID", "PPID", "DEP", "PROCESS TREE");
    }
    if (has_colors()) attroff(COLOR_PAIR(CP_HEADER));
    row++;

    /* ------------------------------------------------------------------ */
    /* Calculate row budget for the scrollable list area.                   */
    /* ------------------------------------------------------------------ */
    int help_row     = max_y - 1;
    int section_row  = max_y - BOTTOM_RESERVE; /* Separator line */
    if (section_row <= row) {
        section_row = row; /* Tiny terminal: best effort */
    }
    int list_end_row  = section_row; /* Exclusive upper bound for list rows */
    int max_list_rows = list_end_row - row;
    if (max_list_rows < 0) max_list_rows = 0;

    /* Adjust scroll offset so the selected row is always visible. */
    if (display_count > 0 && max_list_rows > 0) {
        if (g_selected_index < g_scroll_top) {
            g_scroll_top = g_selected_index;
        }
        if (g_selected_index >= g_scroll_top + max_list_rows) {
            g_scroll_top = g_selected_index - max_list_rows + 1;
        }
        if (g_scroll_top < 0) {
            g_scroll_top = 0;
        }
    }

    /* ------------------------------------------------------------------ */
    /* Render the process list or tree view.                                 */
    /* ------------------------------------------------------------------ */
    if (g_view_mode == 0) {
        /* Process list */
        for (int i = g_scroll_top; i < process_count && row < list_end_row;
                i++, row++) {
            const ProcessInfo *p = &processes[i];
            int selected = (i == g_selected_index);

            mvhline(row, 0, ' ', max_x);

            if (selected) {
                if (has_colors()) attron(COLOR_PAIR(CP_SELECTED));
                else              attron(A_REVERSE);
            }

            /* Truncate name to fit the column width. */
            char name_buf[21];
            strncpy(name_buf, p->name, 20);
            name_buf[20] = '\0';

            safe_print(row, 0, max_x,
                       " %-7d %-7d %-20s %-5c %7.1f %10ld %5d",
                       (int)p->pid,
                       (int)p->ppid,
                       name_buf,
                       (p->state != '\0') ? p->state : '?',
                       p->cpu_usage_percent,
                       p->memory_kb,
                       p->priority);

            if (selected) {
                if (has_colors()) attroff(COLOR_PAIR(CP_SELECTED));
                else              attroff(A_REVERSE);
            }
        }
    } else {
        /* Tree view */
        for (int i = g_scroll_top; i < node_count && row < list_end_row;
                i++, row++) {
            const ProcessTreeNode *n = &nodes[i];
            int selected = (i == g_selected_index);

            mvhline(row, 0, ' ', max_x);

            if (selected) {
                if (has_colors()) attron(COLOR_PAIR(CP_SELECTED));
                else              attron(A_REVERSE);
            }

            /* Build indentation from depth; cap at 16 to avoid overflow. */
            int depth = n->depth;
            if (depth < 0)  depth = 0;
            if (depth > 16) depth = 16;

            char indent[64];
            indent[0] = '\0';
            for (int d = 0; d < depth; d++) {
                strncat(indent, "  ", sizeof(indent) - strlen(indent) - 1);
            }

            safe_print(row, 0, max_x,
                       " %-7d %-7d %3d  %s+- %d",
                       (int)n->pid,
                       (int)n->ppid,
                       n->depth,
                       indent,
                       (int)n->pid);

            if (selected) {
                if (has_colors()) attroff(COLOR_PAIR(CP_SELECTED));
                else              attroff(A_REVERSE);
            }
        }
    }

    /* Fill any remaining list rows with blank lines. */
    while (row < list_end_row) {
        mvhline(row, 0, ' ', max_x);
        row++;
    }

    /* ------------------------------------------------------------------ */
    /* Section separator                                                      */
    /* ------------------------------------------------------------------ */
    if (section_row < max_y) {
        if (has_colors()) attron(COLOR_PAIR(CP_SECTION));
        mvhline(section_row, 0, ACS_HLINE, max_x);
        const char *sec_label = g_log_view
            ? "  EVENT LOG (file: logs/process_guardian.csv)  "
            : "  ANOMALY ALERTS  ";
        mvprintw(section_row, 2, "%s", sec_label);
        if (has_colors()) attroff(COLOR_PAIR(CP_SECTION));
        row = section_row + 1;
    }

    /* ------------------------------------------------------------------ */
    /* Anomaly / log section (up to 2 rows before the help line).           */
    /* ------------------------------------------------------------------ */
    if (!g_log_view) {
        if (anomaly_count == 0 && row < help_row) {
            safe_print(row, 0, max_x, "  No active anomalies detected.");
            clrtoeol();
            row++;
        } else {
            for (int i = 0; i < anomaly_count && row < help_row; i++, row++) {
                if (has_colors()) attron(COLOR_PAIR(CP_ALERT));
                safe_print(row, 0, max_x,
                           "  [%s] PID:%d val:%.1f thr:%.1f  %s",
                           anomalies[i].type,
                           (int)anomalies[i].pid,
                           anomalies[i].measured_value,
                           anomalies[i].threshold,
                           anomalies[i].message);
                clrtoeol();
                if (has_colors()) attroff(COLOR_PAIR(CP_ALERT));
            }
        }
    } else {
        /* Log view: advise user to open the CSV file for full details. */
        if (row < help_row) {
            safe_print(row, 0, max_x,
                       "  Events are written to: logs/process_guardian.csv");
            clrtoeol();
            row++;
        }
    }

    /* ------------------------------------------------------------------ */
    /* Help line (bottom row)                                                */
    /* ------------------------------------------------------------------ */
    if (help_row >= 0 && help_row < max_y) {
        if (has_colors()) attron(COLOR_PAIR(CP_HEADER));
        mvhline(help_row, 0, ' ', max_x);
        safe_print(help_row, 0, max_x,
                   " q:quit  t:tree  s:stop  c:cont  k:kill  p:nice"
                   "  l:log  UP/DN:select");
        if (has_colors()) attroff(COLOR_PAIR(CP_HEADER));
    }

    refresh();
}

/* -------------------------------------------------------------------------
 * dashboard_get_input
 *
 * Reads one key (non-blocking).  Updates internal state for:
 *   KEY_UP / KEY_DOWN -- move row selection
 *   't'               -- toggle process list / tree view
 *   'l'               -- toggle anomaly / log section
 *
 * All other keys (q, s, c, k, p) are returned unchanged for main.c to
 * dispatch.  Returns ERR when no key is pending.
 * ------------------------------------------------------------------------- */
int dashboard_get_input(void)
{
    if (!g_initialized) {
        return ERR;
    }

    int ch = getch();

    switch (ch) {
    case KEY_UP:
        if (g_selected_index > 0) {
            g_selected_index--;
        }
        break;

    case KEY_DOWN:
        /*
         * Increment unconditionally; dashboard_render() will clamp to the
         * actual list size on the next frame.
         */
        g_selected_index++;
        break;

    case 't':
        /* Toggle between process-list and tree-view; reset selection. */
        g_view_mode      = 1 - g_view_mode;
        g_selected_index = 0;
        g_scroll_top     = 0;
        break;

    case 'l':
        /* Toggle between anomaly and log message in the bottom section. */
        g_log_view = 1 - g_log_view;
        break;

    default:
        break;
    }

    return ch;
}

/* -------------------------------------------------------------------------
 * dashboard_get_selected_pid
 *
 * Returns the PID of the currently highlighted row, updated at every
 * dashboard_render() call.  Returns -1 when no row is selected.
 * ------------------------------------------------------------------------- */
int dashboard_get_selected_pid(void)
{
    return g_selected_pid;
}
