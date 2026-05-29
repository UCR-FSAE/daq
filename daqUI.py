"""
DAQ UI — Real-time telemetry dashboard for CAN bus data over serial.

Install dependencies:
    pip install pyqt5 pyqtgraph pyserial

Run:
    python daqUI.py --port COM3           # Windows
    python daqUI.py --port /dev/ttyUSB0   # Linux/Mac
"""

import argparse
import collections
import csv
import sys
import threading
import time
from datetime import datetime

import pyqtgraph as pg
from PyQt5.QtCore import Qt, QTimer, pyqtSignal, QObject
from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QLabel, QPushButton, QGroupBox, QScrollArea,
    QFrame, QSizePolicy, QStatusBar,
)

# ── Thresholds (edit these for your car) ─────────────────────────────────────

THRESHOLDS = {
    "highestTemp": {"warn": 55.0,  "crit": 65.0,  "label": "Highest Cell Temp"},
    "motorTemp":   {"warn": 75.0,  "crit": 90.0,  "label": "Motor Temp"},
    "motorSpeed":  {"warn": 5000,  "crit": 6500,  "label": "Motor Speed"},
    "dcV":         {"warn_lo": 38, "crit_lo": 34, "warn_hi": 58, "crit_hi": 62, "label": "DC Bus Voltage"},
    "sys12V":      {"warn_lo": 10.5, "crit_lo": 9.5, "label": "System 12V"},
    "dcI":         {"warn": 150.0, "crit": 200.0, "label": "DC Bus Current"},
}

SIGNAL_LOST_TIMEOUT = 3.0   # seconds before "signal lost" alert fires
BAUD_RATE = 115200           # match TELEM_BAUD in firmware

# ── Colour palette ────────────────────────────────────────────────────────────

BG     = "#0d0f14"
PANEL  = "#13161e"
BORDER = "#1e2330"
TEXT   = "#c8cdd8"
MUTED  = "#5a6070"
GREEN  = "#2ecc71"
AMBER  = "#f39c12"
RED    = "#e74c3c"
BLUE   = "#3498db"
PURPLE = "#9b59b6"
TEAL   = "#1abc9c"
PINK   = "#e91e8c"

SIGNAL_COLORS = {
    "highestTemp": RED,
    "motorTemp":   AMBER,
    "motorSpeed":  BLUE,
    "dcV":         TEAL,
    "dcI":         PINK,
    "sys12V":      PURPLE,
    "cmdTorq":     RED,
    "torqFb":      GREEN,
    "iA":          BLUE,
    "iB":          AMBER,
    "iC":          TEAL,
    "outV":        PURPLE,
    "pwrOnSec":    MUTED,
}

# ── Plot groups ───────────────────────────────────────────────────────────────

PLOT_GROUPS = [
    {
        "title": "Temperature (°C)",
        "signals": [("highestTemp", "Cell Max"), ("motorTemp", "Motor")],
    },
    {
        "title": "Motor Speed (RPM)",
        "signals": [("motorSpeed", "RPM")],
    },
    {
        "title": "DC Bus",
        "signals": [("dcV", "Voltage (V)"), ("dcI", "Current (A)")],
    },
    {
        "title": "Torque (Nm)",
        "signals": [("cmdTorq", "Commanded"), ("torqFb", "Feedback")],
    },
    {
        "title": "Phase Currents (A)",
        "signals": [("iA", "Phase A"), ("iB", "Phase B"), ("iC", "Phase C")],
    },
    {
        "title": "Auxiliary",
        "signals": [("sys12V", "12V Rail"), ("outV", "Output V")],
    },
]

ALL_KEYS = list({s for g in PLOT_GROUPS for s, _ in g["signals"]})
ALL_KEYS += ["postFaultLo", "postFaultHi", "runFaultLo", "runFaultHi",
             "invState", "invLock", "pFaultLo", "pFaultHi", "pwrOnSec", "pwrOnCnt"]

HISTORY = 300  # samples kept in the rolling window

# ── Shared state ──────────────────────────────────────────────────────────────

data_lock    = threading.Lock()
history      = {k: collections.deque([None] * HISTORY, maxlen=HISTORY) for k in ALL_KEYS}
latest       = {k: None for k in ALL_KEYS}
last_update  = {"t": 0.0}
parse_errors = [0]


# ── Parser ────────────────────────────────────────────────────────────────────

def parse_line(line: str) -> dict:
    result = {}
    for field in line.strip().split(","):
        if "=" not in field:
            continue
        key, _, val = field.partition("=")
        try:
            result[key.strip()] = float(val.strip())
        except ValueError:
            pass
    return result


# ── Serial reader thread ──────────────────────────────────────────────────────

def serial_reader(port, baud, stop_event):
    import serial
    ser = None
    while not stop_event.is_set():
        try:
            if ser is None or not ser.is_open:
                print(f"[serial] Connecting to {port} @ {baud}…", flush=True)
                ser = serial.Serial(port, baud, timeout=1)
                print("[serial] Connected.", flush=True)
        except Exception as e:
            print(f"[serial] Cannot open port: {e}. Retrying in 2s…", flush=True)
            time.sleep(2)
            continue
        try:
            raw = ser.readline()
            if not raw:
                continue
            line = raw.decode("ascii", errors="replace")
        except Exception as e:
            print(f"[serial] Read error: {e}. Reconnecting…", flush=True)
            try:
                ser.close()
            except Exception:
                pass
            ser = None
            continue

        parsed = parse_line(line)
        if not parsed:
            parse_errors[0] += 1
            continue

        with data_lock:
            for key in ALL_KEYS:
                history[key].append(parsed.get(key))
                if key in parsed:
                    latest[key] = parsed[key]
            last_update["t"] = time.time()


# ── Qt signal bridge ──────────────────────────────────────────────────────────

class Signals(QObject):
    alert = pyqtSignal(str, str)


# ── Stat card ─────────────────────────────────────────────────────────────────

class StatCard(QFrame):
    def __init__(self, key, label, unit=""):
        super().__init__()
        self.key  = key
        self.unit = unit
        self.setFixedHeight(72)
        self.setStyleSheet(f"""
            StatCard {{
                background: {PANEL};
                border: 1px solid {BORDER};
                border-radius: 6px;
            }}
        """)
        layout = QVBoxLayout(self)
        layout.setContentsMargins(10, 6, 10, 6)
        layout.setSpacing(2)

        self.lbl = QLabel(label)
        self.lbl.setStyleSheet(f"color: {MUTED}; font-size: 11px; font-family: monospace;")

        self.val = QLabel("—")
        color = SIGNAL_COLORS.get(key, TEXT)
        self.val.setStyleSheet(f"color: {color}; font-size: 20px; font-weight: bold; font-family: monospace;")

        layout.addWidget(self.lbl)
        layout.addWidget(self.val)

    def update_value(self, v):
        if v is None:
            self.val.setText("—")
            self.val.setStyleSheet(f"color: {MUTED}; font-size: 20px; font-weight: bold; font-family: monospace;")
            return

        color = SIGNAL_COLORS.get(self.key, TEXT)
        t = THRESHOLDS.get(self.key)
        if t:
            crit = warn = False
            if "crit"    in t and v >= t["crit"]:    crit = True
            elif "warn"  in t and v >= t["warn"]:    warn = True
            if "crit_lo" in t and v <= t["crit_lo"]: crit = True
            elif "warn_lo" in t and v <= t["warn_lo"]: warn = True
            if "crit_hi" in t and v >= t["crit_hi"]: crit = True
            elif "warn_hi" in t and v >= t["warn_hi"]: warn = True
            if crit:   color = RED
            elif warn: color = AMBER

        unit = f" {self.unit}" if self.unit else ""
        self.val.setText(f"{v:.1f}{unit}")
        self.val.setStyleSheet(f"color: {color}; font-size: 20px; font-weight: bold; font-family: monospace;")


# ── Alert row ─────────────────────────────────────────────────────────────────

class AlertRow(QFrame):
    def __init__(self, level, message, timestamp):
        super().__init__()
        color = RED if level == "CRIT" else AMBER if level == "WARN" else BLUE
        self.setStyleSheet(f"""
            AlertRow {{
                background: {PANEL};
                border-left: 3px solid {color};
                border-radius: 3px;
                margin-bottom: 2px;
            }}
        """)
        layout = QHBoxLayout(self)
        layout.setContentsMargins(8, 4, 8, 4)

        badge = QLabel(level)
        badge.setFixedWidth(38)
        badge.setStyleSheet(f"color: {color}; font-size: 10px; font-weight: bold; font-family: monospace;")

        msg = QLabel(message)
        msg.setStyleSheet(f"color: {TEXT}; font-size: 11px; font-family: monospace;")
        msg.setWordWrap(True)

        ts = QLabel(timestamp)
        ts.setStyleSheet(f"color: {MUTED}; font-size: 10px; font-family: monospace;")
        ts.setAlignment(Qt.AlignRight | Qt.AlignVCenter)

        layout.addWidget(badge)
        layout.addWidget(msg, 1)
        layout.addWidget(ts)


# ── Main window ───────────────────────────────────────────────────────────────

class DAQUI(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("DAQ UI — Motor Inverter Telemetry")
        self.resize(1400, 900)
        self._setup_style()

        self.sig            = Signals()
        self.sig.alert.connect(self._add_alert)
        self._alert_states  = {}
        self._session_start = datetime.now()
        self._logging       = False
        self._log_file      = None
        self._log_writer    = None

        central = QWidget()
        self.setCentralWidget(central)
        root = QHBoxLayout(central)
        root.setContentsMargins(8, 8, 8, 8)
        root.setSpacing(8)

        # Left: plots
        left = QWidget()
        left_layout = QVBoxLayout(left)
        left_layout.setContentsMargins(0, 0, 0, 0)
        left_layout.setSpacing(6)
        left_layout.addWidget(self._make_header())
        left_layout.addWidget(self._make_stat_cards())

        self._plot_widget = pg.GraphicsLayoutWidget()
        self._plot_widget.setBackground(PANEL)
        self._plot_widget.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        left_layout.addWidget(self._plot_widget, 1)
        self._build_plots()

        # Right: faults + alerts + logging
        right = QWidget()
        right.setFixedWidth(320)
        right_layout = QVBoxLayout(right)
        right_layout.setContentsMargins(0, 0, 0, 0)
        right_layout.setSpacing(6)
        right_layout.addWidget(self._make_fault_panel())
        right_layout.addWidget(self._make_alert_panel(), 1)
        right_layout.addWidget(self._make_log_controls())

        root.addWidget(left, 1)
        root.addWidget(right)

        # Status bar
        self.status = QStatusBar()
        self.status.setStyleSheet(f"background: {PANEL}; color: {MUTED}; font-size: 10px; font-family: monospace;")
        self.setStatusBar(self.status)
        self._status_sig  = QLabel("● WAITING")
        self._status_err  = QLabel("parse errors: 0")
        self._status_time = QLabel("")
        self.status.addWidget(self._status_sig)
        self.status.addPermanentWidget(self._status_err)
        self.status.addPermanentWidget(self._status_time)

        self._timer = QTimer()
        self._timer.timeout.connect(self._refresh)
        self._timer.start(100)

    # ── Style ─────────────────────────────────────────────────────────────────

    def _setup_style(self):
        self.setStyleSheet(f"""
            QMainWindow, QWidget {{ background: {BG}; color: {TEXT}; }}
            QGroupBox {{
                border: 1px solid {BORDER}; border-radius: 6px;
                margin-top: 8px; font-size: 11px; color: {MUTED}; font-family: monospace;
            }}
            QGroupBox::title {{ subcontrol-origin: margin; left: 8px; padding: 0 4px; }}
            QPushButton {{
                background: {PANEL}; border: 1px solid {BORDER}; border-radius: 4px;
                color: {TEXT}; font-family: monospace; font-size: 11px; padding: 4px 10px;
            }}
            QPushButton:hover {{ border-color: {BLUE}; color: {BLUE}; }}
            QPushButton:pressed {{ background: {BORDER}; }}
            QScrollArea {{ border: none; background: transparent; }}
            QScrollBar:vertical {{ background: {PANEL}; width: 6px; border-radius: 3px; }}
            QScrollBar::handle:vertical {{ background: {BORDER}; border-radius: 3px; }}
        """)

    # ── Header ────────────────────────────────────────────────────────────────

    def _make_header(self):
        w = QFrame()
        w.setFixedHeight(36)
        w.setStyleSheet(f"background: {PANEL}; border: 1px solid {BORDER}; border-radius: 6px;")
        layout = QHBoxLayout(w)
        layout.setContentsMargins(12, 0, 12, 0)

        title = QLabel("◈  DAQ UI")
        title.setStyleSheet(f"color: {TEXT}; font-size: 13px; font-weight: bold; font-family: monospace;")

        live = QLabel("LIVE")
        live.setStyleSheet(f"color: {GREEN}; font-size: 10px; font-family: monospace;")

        self._session_label = QLabel("")
        self._session_label.setStyleSheet(f"color: {MUTED}; font-size: 10px; font-family: monospace;")

        layout.addWidget(title)
        layout.addStretch()
        layout.addWidget(self._session_label)
        layout.addSpacing(16)
        layout.addWidget(live)
        return w

    # ── Stat cards ────────────────────────────────────────────────────────────

    def _make_stat_cards(self):
        w = QWidget()
        layout = QHBoxLayout(w)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(6)
        self._cards = {}
        defs = [
            ("highestTemp", "CELL MAX",   "°C"),
            ("motorTemp",   "MOTOR",      "°C"),
            ("motorSpeed",  "SPEED",      "RPM"),
            ("dcV",         "DC BUS V",   "V"),
            ("dcI",         "DC BUS I",   "A"),
            ("sys12V",      "12V RAIL",   "V"),
            ("cmdTorq",     "CMD TORQUE", "Nm"),
            ("torqFb",      "TQ FDBK",   "Nm"),
        ]
        for key, label, unit in defs:
            card = StatCard(key, label, unit)
            self._cards[key] = card
            layout.addWidget(card)
        return w

    # ── Plots ─────────────────────────────────────────────────────────────────

    def _build_plots(self):
        self._curves = {}
        pw = self._plot_widget
        for i, group in enumerate(PLOT_GROUPS):
            r, c = divmod(i, 2)
            plot = pw.addPlot(row=r, col=c, title=group["title"])
            plot.titleLabel.setText(
                f"<span style='color:{MUTED}; font-size:10px; font-family:monospace'>{group['title']}</span>"
            )
            plot.getAxis("left").setPen(pg.mkPen(BORDER))
            plot.getAxis("bottom").setPen(pg.mkPen(BORDER))
            plot.getAxis("left").setTextPen(pg.mkPen(MUTED))
            plot.getAxis("bottom").setTextPen(pg.mkPen(MUTED))
            plot.showGrid(x=True, y=True, alpha=0.15)
            plot.setMenuEnabled(False)
            for key, label in group["signals"]:
                color = SIGNAL_COLORS.get(key, TEXT)
                curve = plot.plot(pen=pg.mkPen(color=color, width=1.5), name=label)
                self._curves[key] = curve
            if len(group["signals"]) > 1:
                legend = plot.addLegend(offset=(5, 5))
                legend.setLabelTextColor(MUTED)

    # ── Fault panel ───────────────────────────────────────────────────────────

    def _make_fault_panel(self):
        box = QGroupBox("FAULT REGISTERS")
        layout = QVBoxLayout(box)
        layout.setSpacing(4)
        self._fault_labels = {}
        faults = [
            ("postFaultLo", "Post Fault Lo"),
            ("postFaultHi", "Post Fault Hi"),
            ("runFaultLo",  "Run Fault Lo"),
            ("runFaultHi",  "Run Fault Hi"),
            ("invState",    "Inverter State"),
            ("invLock",     "Inv Lockout"),
        ]
        for key, label in faults:
            row = QHBoxLayout()
            name_lbl = QLabel(label)
            name_lbl.setStyleSheet(f"color: {MUTED}; font-size: 10px; font-family: monospace;")
            val_lbl = QLabel("—")
            val_lbl.setStyleSheet(f"color: {TEXT}; font-size: 10px; font-family: monospace;")
            val_lbl.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
            row.addWidget(name_lbl)
            row.addStretch()
            row.addWidget(val_lbl)
            layout.addLayout(row)
            self._fault_labels[key] = val_lbl
        return box

    # ── Alert panel ───────────────────────────────────────────────────────────

    def _make_alert_panel(self):
        box = QGroupBox("ALERTS")
        layout = QVBoxLayout(box)
        layout.setContentsMargins(6, 12, 6, 6)
        layout.setSpacing(4)

        btn_row = QHBoxLayout()
        clear_btn = QPushButton("Clear")
        clear_btn.clicked.connect(self._clear_alerts)
        btn_row.addStretch()
        btn_row.addWidget(clear_btn)
        layout.addLayout(btn_row)

        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarAlwaysOff)

        self._alert_container = QWidget()
        self._alert_layout = QVBoxLayout(self._alert_container)
        self._alert_layout.setContentsMargins(0, 0, 0, 0)
        self._alert_layout.setSpacing(2)
        self._alert_layout.addStretch()

        scroll.setWidget(self._alert_container)
        layout.addWidget(scroll, 1)
        return box

    def _add_alert(self, level, message):
        ts = datetime.now().strftime("%H:%M:%S")
        row = AlertRow(level, message, ts)
        self._alert_layout.insertWidget(self._alert_layout.count() - 1, row)
        if self._alert_layout.count() > 52:
            item = self._alert_layout.takeAt(0)
            if item and item.widget():
                item.widget().deleteLater()

    def _clear_alerts(self):
        while self._alert_layout.count() > 1:
            item = self._alert_layout.takeAt(0)
            if item and item.widget():
                item.widget().deleteLater()

    # ── Log controls ─────────────────────────────────────────────────────────

    def _make_log_controls(self):
        box = QGroupBox("SESSION LOGGING")
        layout = QVBoxLayout(box)
        layout.setSpacing(4)

        self._log_status = QLabel("Not logging")
        self._log_status.setStyleSheet(f"color: {MUTED}; font-size: 10px; font-family: monospace;")

        self._log_btn = QPushButton("▶  Start Log")
        self._log_btn.clicked.connect(self._toggle_log)

        layout.addWidget(self._log_status)
        layout.addWidget(self._log_btn)
        return box

    def _toggle_log(self):
        if not self._logging:
            fname = datetime.now().strftime("telem_%Y%m%d_%H%M%S.csv")
            self._log_file   = open(fname, "w", newline="")
            fieldnames       = ["timestamp"] + ALL_KEYS
            self._log_writer = csv.DictWriter(self._log_file, fieldnames=fieldnames)
            self._log_writer.writeheader()
            self._logging    = True
            self._log_btn.setText("■  Stop Log")
            self._log_status.setText(f"Logging → {fname}")
            self._log_status.setStyleSheet(f"color: {GREEN}; font-size: 10px; font-family: monospace;")
        else:
            self._logging = False
            if self._log_file:
                self._log_file.close()
                self._log_file   = None
                self._log_writer = None
            self._log_btn.setText("▶  Start Log")
            self._log_status.setText("Log saved.")
            self._log_status.setStyleSheet(f"color: {MUTED}; font-size: 10px; font-family: monospace;")

    # ── Refresh ───────────────────────────────────────────────────────────────

    def _refresh(self):
        with data_lock:
            snap_history = {k: list(history[k]) for k in ALL_KEYS}
            snap_latest  = dict(latest)
            snap_lu      = last_update["t"]
            snap_errors  = parse_errors[0]

        # Update curves
        for key, curve in self._curves.items():
            xdata, ydata = [], []
            for i, v in enumerate(snap_history[key]):
                if v is not None:
                    xdata.append(i)
                    ydata.append(v)
            curve.setData(xdata, ydata)

        # Update stat cards
        for key, card in self._cards.items():
            card.update_value(snap_latest.get(key))

        # Update fault labels
        any_fault = False
        for key, lbl in self._fault_labels.items():
            v = snap_latest.get(key)
            if v is None:
                lbl.setText("—")
                lbl.setStyleSheet(f"color: {MUTED}; font-size: 10px; font-family: monospace;")
                continue
            iv = int(v)
            if key in ("postFaultLo", "postFaultHi", "runFaultLo", "runFaultHi"):
                text  = f"0x{iv:08X}"
                color = RED if iv != 0 else GREEN
                if iv != 0:
                    any_fault = True
            elif key == "invLock":
                text  = "LOCKED" if iv else "OK"
                color = AMBER if iv else GREEN
            else:
                text  = str(iv)
                color = TEXT
            lbl.setText(text)
            lbl.setStyleSheet(f"color: {color}; font-size: 10px; font-family: monospace;")

        # Threshold alerts
        self._check_thresholds(snap_latest)

        # Fault register alert
        if any_fault:
            self._maybe_alert("fault_reg", "CRIT", "Fault register non-zero!")
        else:
            self._alert_states.pop("fault_reg", None)

        # Signal lost
        signal_ok = (time.time() - snap_lu) < SIGNAL_LOST_TIMEOUT if snap_lu > 0 else False
        if not signal_ok and snap_lu > 0:
            self._maybe_alert("signal_lost", "WARN", "Signal lost — no data received")
            self._status_sig.setText("● NO SIGNAL")
            self._status_sig.setStyleSheet(f"color: {RED}; font-size: 10px; font-family: monospace;")
        else:
            self._alert_states.pop("signal_lost", None)
            self._status_sig.setText("● SIGNAL OK" if snap_lu > 0 else "● WAITING")
            color = GREEN if snap_lu > 0 else MUTED
            self._status_sig.setStyleSheet(f"color: {color}; font-size: 10px; font-family: monospace;")

        # Status bar
        self._status_err.setText(f"parse errors: {snap_errors}")
        elapsed = datetime.now() - self._session_start
        h, rem  = divmod(int(elapsed.total_seconds()), 3600)
        m, s    = divmod(rem, 60)
        self._session_label.setText(f"session  {h:02d}:{m:02d}:{s:02d}")
        self._status_time.setText(datetime.now().strftime("%H:%M:%S"))

        # Log
        if self._logging and self._log_writer:
            row = {"timestamp": datetime.now().isoformat()}
            row.update({k: snap_latest.get(k) for k in ALL_KEYS})
            self._log_writer.writerow(row)

    def _check_thresholds(self, snap):
        for key, t in THRESHOLDS.items():
            v = snap.get(key)
            if v is None:
                self._alert_states.pop(key, None)
                continue
            level = msg = None
            if "crit"    in t and v >= t["crit"]:    level, msg = "CRIT", f"{t['label']} critical: {v:.1f}"
            elif "warn"  in t and v >= t["warn"]:    level, msg = "WARN", f"{t['label']} warning: {v:.1f}"
            if "crit_lo" in t and v <= t["crit_lo"]: level, msg = "CRIT", f"{t['label']} critically low: {v:.1f}"
            elif "warn_lo" in t and v <= t["warn_lo"]: level, msg = "WARN", f"{t['label']} low: {v:.1f}"
            if "crit_hi" in t and v >= t["crit_hi"]: level, msg = "CRIT", f"{t['label']} critically high: {v:.1f}"
            elif "warn_hi" in t and v >= t["warn_hi"]: level, msg = "WARN", f"{t['label']} high: {v:.1f}"
            if level:
                self._maybe_alert(key, level, msg)
            else:
                self._alert_states.pop(key, None)

    def _maybe_alert(self, key, level, msg):
        if self._alert_states.get(key) != level:
            self._alert_states[key] = level
            self.sig.alert.emit(level, msg)

    def closeEvent(self, event):
        if self._logging and self._log_file:
            self._log_file.close()
        event.accept()


# ── Entry point ───────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description="DAQ UI — motor inverter telemetry")
    parser.add_argument("--port", default="COM3",      help="Serial port (e.g. COM3, /dev/ttyUSB0)")
    parser.add_argument("--baud", default=BAUD_RATE,   type=int, help="Baud rate")
    args = parser.parse_args()

    stop_event = threading.Event()
    t = threading.Thread(target=serial_reader, args=(args.port, args.baud, stop_event), daemon=True)
    t.start()

    app = QApplication(sys.argv)
    app.setStyle("Fusion")
    win = DAQUI()
    win.show()

    try:
        ret = app.exec_()
    finally:
        stop_event.set()
    sys.exit(ret)


if __name__ == "__main__":
    main()