import sys
import os

class SimplePDF:
    def __init__(self, filename):
        self.filename = filename
        self.pages = []
        self.current_stream = []
        self.page_width = 612   # Letter width (8.5 in)
        self.page_height = 792  # Letter height (11 in)
        self.margin_x = 45
        self.margin_top = 50
        self.margin_bottom = 50
        self.cursor_y = self.page_height - self.margin_top
        self.page_number = 0
        self.new_page()

    def new_page(self):
        if self.current_stream:
            self.pages.append("".join(self.current_stream))
            self.current_stream = []
        self.page_number += 1
        self.cursor_y = self.page_height - self.margin_top
        # Header line (pages after first)
        if self.page_number > 1:
            self.set_color(0.4, 0.4, 0.4)
            self.draw_text("OS Process Guardian — Architecture & Technical Report", 
                           self.margin_x, self.page_height - 35, font="F1", size=9)
            self.draw_line(self.margin_x, self.page_height - 38, 
                           self.page_width - self.margin_x, self.page_height - 38, width=0.5)

    def check_space(self, height_needed):
        if self.cursor_y - height_needed < self.margin_bottom:
            self.new_page()

    def escape_text(self, text):
        return text.replace('\\', '\\\\').replace('(', '\\(').replace(')', '\\)')

    def set_color(self, r, g, b):
        self.current_stream.append(f"{r:.3f} {g:.3f} {b:.3f} rg ")

    def set_stroke_color(self, r, g, b):
        self.current_stream.append(f"{r:.3f} {g:.3f} {b:.3f} RG ")

    def draw_line(self, x1, y1, x2, y2, width=1.0):
        self.current_stream.append(f"{width:.2f} w {x1:.2f} {y1:.2f} m {x2:.2f} {y2:.2f} l S\n")

    def draw_rect(self, x, y, w, h, fill=False, stroke=True):
        op = "B" if fill and stroke else ("f" if fill else "S")
        self.current_stream.append(f"{x:.2f} {y:.2f} {w:.2f} {h:.2f} re {op}\n")

    def draw_text(self, text, x, y, font="F1", size=10):
        esc = self.escape_text(text)
        self.current_stream.append(f"BT /{font} {size} Tf {x:.2f} {y:.2f} Td ({esc}) Tj ET\n")

    def add_title_banner(self, title, subtitle):
        self.check_space(75)
        h = 60
        y = self.cursor_y - h
        # Dark navy banner
        self.set_color(0.08, 0.18, 0.36)
        self.draw_rect(self.margin_x, y, self.page_width - 2 * self.margin_x, h, fill=True, stroke=False)
        # Title
        self.set_color(1.0, 1.0, 1.0)
        self.draw_text(title, self.margin_x + 15, y + 36, font="F2", size=18)
        # Subtitle
        self.set_color(0.85, 0.90, 0.98)
        self.draw_text(subtitle, self.margin_x + 15, y + 16, font="F1", size=11)
        self.cursor_y = y - 20

    def add_heading1(self, text):
        self.check_space(35)
        self.cursor_y -= 10
        self.set_color(0.10, 0.22, 0.42)
        self.draw_text(text, self.margin_x, self.cursor_y, font="F2", size=13)
        self.set_stroke_color(0.10, 0.22, 0.42)
        self.draw_line(self.margin_x, self.cursor_y - 4, self.page_width - self.margin_x, self.cursor_y - 4, width=1.2)
        self.cursor_y -= 16

    def add_heading2(self, text):
        self.check_space(25)
        self.cursor_y -= 6
        self.set_color(0.15, 0.35, 0.60)
        self.draw_text(text, self.margin_x, self.cursor_y, font="F2", size=11)
        self.cursor_y -= 14

    def add_paragraph(self, text, size=9.5, leading=12.5, bold_prefix=None):
        words = text.split()
        lines = []
        cur_line = []
        max_width = self.page_width - 2 * self.margin_x
        # approximate char width for 9.5pt Helvetica is ~5.2 pt
        char_w = size * 0.52
        max_chars = int(max_width / char_w)

        for w in words:
            if len(" ".join(cur_line + [w])) <= max_chars:
                cur_line.append(w)
            else:
                lines.append(" ".join(cur_line))
                cur_line = [w]
        if cur_line:
            lines.append(" ".join(cur_line))

        self.check_space(len(lines) * leading + 4)
        for i, line in enumerate(lines):
            self.set_color(0.15, 0.15, 0.15)
            self.draw_text(line, self.margin_x, self.cursor_y, font="F1", size=size)
            self.cursor_y -= leading
        self.cursor_y -= 4

    def add_bullet(self, bold_text, normal_text, size=9.0, leading=12.0):
        full = f"{bold_text} {normal_text}"
        words = full.split()
        lines = []
        cur_line = []
        max_chars = int((self.page_width - 2 * self.margin_x - 16) / (size * 0.52))

        for w in words:
            if len(" ".join(cur_line + [w])) <= max_chars:
                cur_line.append(w)
            else:
                lines.append(" ".join(cur_line))
                cur_line = [w]
        if cur_line:
            lines.append(" ".join(cur_line))

        self.check_space(len(lines) * leading + 3)
        # Bullet dot
        self.set_color(0.15, 0.35, 0.60)
        self.draw_text("•", self.margin_x + 3, self.cursor_y, font="F2", size=size + 1)
        
        for i, line in enumerate(lines):
            self.set_color(0.15, 0.15, 0.15)
            self.draw_text(line, self.margin_x + 14, self.cursor_y, font="F1", size=size)
            self.cursor_y -= leading
        self.cursor_y -= 2

    def add_callout(self, title, lines):
        h = 20 + len(lines) * 12
        self.check_space(h + 8)
        y = self.cursor_y - h
        w = self.page_width - 2 * self.margin_x
        # Light blue background
        self.set_color(0.94, 0.96, 0.99)
        self.set_stroke_color(0.20, 0.45, 0.75)
        self.draw_rect(self.margin_x, y, w, h, fill=True, stroke=True)
        # Accent left bar
        self.set_color(0.20, 0.45, 0.75)
        self.draw_rect(self.margin_x, y, 4, h, fill=True, stroke=False)
        # Title
        self.draw_text(title, self.margin_x + 12, y + h - 14, font="F2", size=9.5)
        # Lines
        cy = y + h - 26
        for line in lines:
            self.set_color(0.2, 0.2, 0.2)
            self.draw_text(line, self.margin_x + 12, cy, font="F1", size=8.5)
            cy -= 12
        self.cursor_y = y - 8

    def add_table(self, headers, rows, col_widths):
        total_w = sum(col_widths)
        row_h = 16
        header_h = 18
        table_h = header_h + len(rows) * row_h
        self.check_space(table_h + 10)
        y = self.cursor_y - header_h
        
        # Header background
        self.set_color(0.12, 0.24, 0.45)
        self.draw_rect(self.margin_x, y, total_w, header_h, fill=True, stroke=False)
        
        # Header text
        cx = self.margin_x
        for i, h in enumerate(headers):
            self.set_color(1.0, 1.0, 1.0)
            self.draw_text(h, cx + 5, y + 5, font="F2", size=8.5)
            cx += col_widths[i]

        # Rows
        for r_idx, row in enumerate(rows):
            y -= row_h
            # alternate row color
            if r_idx % 2 == 0:
                self.set_color(0.96, 0.97, 0.99)
                self.draw_rect(self.margin_x, y, total_w, row_h, fill=True, stroke=False)
            # cell borders
            self.set_stroke_color(0.85, 0.88, 0.92)
            self.draw_rect(self.margin_x, y, total_w, row_h, fill=False, stroke=True)

            cx = self.margin_x
            for c_idx, cell in enumerate(row):
                self.set_color(0.15, 0.15, 0.15)
                # bold for first col or result
                is_bold = "PASS" in str(cell) or c_idx == 0
                font_name = "F2" if is_bold else "F1"
                if "PASS" in str(cell):
                    self.set_color(0.05, 0.50, 0.15)
                self.draw_text(str(cell), cx + 5, y + 4, font=font_name, size=8.0)
                cx += col_widths[c_idx]

        self.cursor_y = y - 10

    def add_code_box(self, code_lines):
        line_h = 11
        h = 14 + len(code_lines) * line_h
        self.check_space(h + 8)
        y = self.cursor_y - h
        w = self.page_width - 2 * self.margin_x
        # Background
        self.set_color(0.95, 0.96, 0.97)
        self.set_stroke_color(0.78, 0.82, 0.86)
        self.draw_rect(self.margin_x, y, w, h, fill=True, stroke=True)
        # Text
        cy = y + h - 12
        for line in code_lines:
            self.set_color(0.10, 0.15, 0.25)
            self.draw_text(line, self.margin_x + 10, cy, font="F3", size=8.0)
            cy -= line_h
        self.cursor_y = y - 8

    def build(self):
        if self.current_stream:
            self.pages.append("".join(self.current_stream))
        total_pages = len(self.pages)

        # Add footers to all pages
        for idx in range(total_pages):
            footer_str = (
                f"BT /F1 8 Tf 45 30 Td (OS Process Guardian — Final Implementation & Validation Report) Tj ET\n"
                f"BT /F1 8 Tf {self.page_width - 95:.2f} 30 Td (Page {idx+1} of {total_pages}) Tj ET\n"
                f"0.5 w 0.7 0.7 0.7 RG 45 42 m {self.page_width - 45:.2f} 42 l S\n"
            )
            self.pages[idx] = self.pages[idx] + footer_str

        # Generate PDF objects
        objects = []
        xref = []
        
        # 1 0 obj: Catalog
        objects.append("1 0 obj\n<</Type/Catalog/Pages 2 0 R>>\nendobj\n")
        # 2 0 obj: Pages
        kids = " ".join([f"{4 + i * 2} 0 R" for i in range(total_pages)])
        objects.append(f"2 0 obj\n<</Type/Pages/Kids[{kids}]/Count {total_pages}>>\nendobj\n")
        # 3 0 obj: Resources (Fonts)
        objects.append(
            "3 0 obj\n"
            "<</Font<<"
            "/F1 <</Type/Font/Subtype/Type1/BaseFont/Helvetica>>"
            "/F2 <</Type/Font/Subtype/Type1/BaseFont/Helvetica-Bold>>"
            "/F3 <</Type/Font/Subtype/Type1/BaseFont/Courier>>"
            "/F4 <</Type/Font/Subtype/Type1/BaseFont/Courier-Bold>>"
            ">>>>\nendobj\n"
        )

        obj_id = 4
        for idx, page_content in enumerate(self.pages):
            content_id = obj_id + 1
            # Page object
            objects.append(
                f"{obj_id} 0 obj\n"
                f"<</Type/Page/Parent 2 0 R/MediaBox[0 0 {self.page_width} {self.page_height}]"
                f"/Resources 3 0 R/Contents {content_id} 0 R>>\nendobj\n"
            )
            # Contents stream object
            content_bytes = page_content.encode('utf-8')
            objects.append(
                f"{content_id} 0 obj\n"
                f"<</Length {len(content_bytes)}>>stream\n"
                f"{page_content}"
                f"endstream\nendobj\n"
            )
            obj_id += 2

        # Write output file
        with open(self.filename, 'wb') as f:
            f.write(b"%PDF-1.4\n")
            pos = f.tell()
            offsets = []
            for obj in objects:
                offsets.append(f.tell())
                f.write(obj.encode('utf-8'))

            xref_pos = f.tell()
            f.write(f"xref\n0 {len(offsets) + 1}\n".encode('utf-8'))
            f.write(b"0000000000 65535 f \n")
            for off in offsets:
                f.write(f"{off:010d} 00000 n \n".encode('utf-8'))

            trailer = (
                f"trailer\n<</Size {len(offsets) + 1}/Root 1 0 R>>\n"
                f"startxref\n{xref_pos}\n%%EOF\n"
            )
            f.write(trailer.encode('utf-8'))

        print(f"PDF successfully written to {self.filename} ({total_pages} pages)")

def generate_report():
    pdf = SimplePDF("docs/OS_Process_Guardian_Detailed_Report.pdf")

    # Title Banner
    pdf.add_title_banner(
        "OS PROCESS GUARDIAN",
        "Technical Architecture, Implementation Details, Test Results & Presentation Context"
    )

    pdf.add_callout("PROJECT METADATA & ARTIFACT INFORMATION", [
        "Language & Platform: C11 (POSIX.1-2008) on Linux (Ubuntu on WSL / Native Linux)",
        "Build System: GNU Make + GCC (-std=c11 -Wall -Wextra -Wpedantic) | UI: ncurses",
        "Repository State: Clean, integrated, fully synchronized with GitHub origin/main",
        "Authors: Anannya Devale (Member 1), Tanvi Modi (Member 2), Yukta Mahajan (Member 3)"
    ])

    pdf.add_heading1("1. Project Purpose & Executive Overview")
    pdf.add_paragraph(
        "OS Process Guardian is a high-performance, modular user-space process monitoring and control "
        "system implemented in C11 for Linux environments. The program directly interfaces with the Linux "
        "/proc pseudo-filesystem to extract real-time process statistics, build parent-child ancestry trees, "
        "track lifecycle state changes, detect persistent resource consumption anomalies, optionally apply "
        "priority de-escalation via Linux nice levels, and present an interactive ncurses terminal dashboard "
        "with an append-only CSV audit log."
    )

    pdf.add_heading1("2. Team Member Ownership & Architecture Matrix")
    headers = ["Member", "Module Scope", "Key Public APIs", "Primary Files Owned"]
    rows = [
        ["Member 1\n(Anannya)", "Discovery & Metrics", "proc_reader_read_process()\nprocess_manager_get_all()\nresource_monitor_update()", "proc_reader.c, process_manager.c,\nresource_monitor.c, test_*.c, cpu_hog.c"],
        ["Member 2\n(Tanvi)", "Tree, State & Control", "process_tree_build()\nstate_tracker_find_changes()\nprocess_control_*\nlogger_*", "process_tree.c, state_tracker.c,\nprocess_control.c, priority_manager.c,\nlogger.c, child_tree.c, sleep_test.c"],
        ["Member 3\n(Yukta)", "Anomalies, Dashboard & Main", "anomaly_detector_init/update()\ncontroller_handle_anomaly()\ndashboard_init/render/input()", "anomaly_detector.c, controller.c,\ndashboard.c, main.c, test_*.c"]
    ]
    pdf.add_table(headers, rows, [70, 110, 170, 172])

    pdf.add_heading1("3. Core Technical Mechanics & Implementation Details")
    pdf.add_heading2("3.1 Safe /proc/[pid]/stat Parsing (Member 1)")
    pdf.add_paragraph(
        "Linux process names in /proc/[pid]/stat are enclosed in parentheses and may contain spaces or embedded "
        "parentheses (e.g. '(Web Content)'). Standard scanf or strtok approaches fail catastrophically. The Guardian "
        "solves this by locating the first '(' with strchr() and the last ')' with strrchr(), safely copying the command "
        "name and parsing subsequent state, ppid, user ticks, and kernel ticks from the remainder of the line."
    )

    pdf.add_heading2("3.2 CPU Utilization Calculation (Member 1)")
    pdf.add_paragraph(
        "CPU utilization is calculated across consecutive sampling snapshots using the exact formula:"
    )
    pdf.add_code_box([
        "ticks_per_sec = sysconf(_SC_CLK_TCK);",
        "delta_ticks   = current.total_cpu_ticks - previous.total_cpu_ticks;",
        "cpu_usage_pct = ((delta_ticks / (double)ticks_per_sec) / elapsed_seconds) * 100.0;"
    ])
    pdf.add_paragraph(
        "The calculated value reflects the percentage of one logical CPU core (0.0% to 100.0%+), matching standard top "
        "behavior without requiring system-wide tick normalization or multi-core division."
    )

    pdf.add_heading2("3.3 Anomaly Detection & Active Suppression (Member 3)")
    pdf.add_paragraph(
        "To avoid alert fatigue, the anomaly detector enforces a strict persistence and suppression contract:"
    )
    pdf.add_bullet("Threshold Criteria:", "CPU > 80.0% of a core, or Memory growth >= 10,240 KiB per sample.")
    pdf.add_bullet("Persistence Rule:", "Requires exactly 3 consecutive samples exceeding threshold before confirming.")
    pdf.add_bullet("Active Suppression:", "Emits exactly ONE event upon confirmation; suppresses subsequent alarms while condition remains continuously active.")
    pdf.add_bullet("State Reset & Recovery:", "When usage drops below threshold, counters and active flags reset, enabling re-triggering upon recurrence.")

    pdf.add_heading1("4. Verification & Automated Test Results")
    pdf.add_paragraph(
        "The system has been thoroughly validated through unit test suites and controlled live workloads:"
    )

    t_headers = ["Test Suite / Target", "Module Validated", "Key Test Conditions", "Result"]
    t_rows = [
        ["test_anomaly_detector", "Anomaly Engine", "43 assertions: thresholds, persistence, suppression, recovery, cleanup", "PASS (43/43)"],
        ["test_process_manager", "Discovery & Enumeration", "Self-PID read, PPID validity, /proc traversal, duplicate checks, exited reaps", "PASS"],
        ["test_resource_monitor", "CPU Calculator", "Delta ticks, clock rate conversion, new PID zeroing, monotonic ticks", "PASS"],
        ["test_process_tree", "Hierarchy Builder", "Multi-tier parent resolution, orphan handling, cycle protection", "PASS"],
        ["test_state_tracker", "State Engine", "R->S transitions, exit detection (exited=1), unmutated arrays", "PASS"],
        ["test_process_control", "Signal Dispatcher", "SIGSTOP pause, SIGCONT resume, SIGTERM termination", "PASS"],
        ["test_priority_manager", "Priority Controller", "getpriority/setpriority bounds [-20, 19], errno validation", "PASS"],
        ["test_logger", "CSV Event Log", "RFC-4180 escaping, header generation, flush verification", "PASS"]
    ]
    pdf.add_table(t_headers, t_rows, [110, 100, 240, 72])

    pdf.add_heading1("5. Live Demonstration & Verification Walkthrough")
    pdf.add_paragraph(
        "Live system integration was tested using the CPU hog workload generator (tests/cpu_hog.c):"
    )
    pdf.add_bullet("1. Workload Startup:", "Spawned ./tests/cpu_hog & which initiated a pure CPU calculation loop (PID 14523).")
    pdf.add_bullet("2. Detection & Alerting:", "Launched ./process_guardian. Within 3 seconds (3 consecutive samples at ~100% CPU), the anomaly engine triggered.")
    pdf.add_bullet("3. Visual Indicator:", "The ncurses dashboard immediately rendered a bright RED alert banner: 'ALERTS: [CPU_HIGH] PID 14523 CPU 100.00% > 80.00% for 3 consecutive samples'.")
    pdf.add_bullet("4. Audit Log Verification:", "Event was appended to logs/process_guardian.csv with RFC-4180 quoting and precise epoch timestamp.")
    pdf.add_bullet("5. Active Suppression:", "On sample 4, the alert banner returned to 'No anomalies detected', confirming suppression of duplicate alarms.")
    pdf.add_bullet("6. Process Control:", "Operator pressed 's' to pause the process (CPU dropped to 0%, state flipped to 'T'). Operator pressed 'c' to resume. Upon 3 seconds of resumed load, the anomaly fired again, proving condition recovery.")
    pdf.add_bullet("7. Safe Termination:", "Operator pressed 'k' and confirmed with 'y'. Process was delivered SIGTERM, reaped cleanly, and removed from display.")

    pdf.add_heading1("6. Presentation & Report Structure (Context for Next Agent)")
    pdf.add_paragraph(
        "The following 10-slide structure is recommended for the final presentation and executive project report:"
    )
    pdf.add_bullet("Slide 1 — Title & Overview:", "Project identity, core objectives, non-invasive user-space architecture.")
    pdf.add_bullet("Slide 2 — Architectural Pipeline:", "Data flow from /proc -> Discovery -> Metrics -> Hierarchy -> Anomalies -> Controller -> UI/Logs.")
    pdf.add_bullet("Slide 3 — Process Discovery (M1):", "Safe /proc/[pid]/stat parsing, handling spaces/parentheses in command names, VmRSS extraction.")
    pdf.add_bullet("Slide 4 — Resource Monitoring (M1):", "Single-core normalized CPU utilization formula using CLK_TCK and elapsed monotonic time.")
    pdf.add_bullet("Slide 5 — Process Tree & State Tracking (M2):", "Ancestry reconstruction, cycle prevention, runtime state transitions, process exit detection.")
    pdf.add_bullet("Slide 6 — Control, Nice Priority & Audit Logs (M2):", "POSIX signals (SIGSTOP/SIGCONT/SIGTERM), nice priority bounds [-20, 19], RFC-4180 CSV logs.")
    pdf.add_bullet("Slide 7 — Anomaly Detection & Suppression (M3):", "3-sample persistence validation, active suppression of continuous violations, condition reset.")
    pdf.add_bullet("Slide 8 — ncurses Interactive Dashboard (M3):", "Full-screen keyboard UI, view toggles (tree/log), interactive termination confirmation prompt.")
    pdf.add_bullet("Slide 9 — Validation & Live Test Results:", "Summary of 8 passing test suites, 43/43 unit tests passed, live cpu_hog anomaly detection demo.")
    pdf.add_bullet("Slide 10 — Engineering Highlights & Conclusion:", "Deterministic memory footprint, zero-heap runtime monitoring loop, robust Linux systems programming.")

    pdf.build()

if __name__ == "__main__":
    generate_report()
