#!/usr/bin/env python3
"""End-to-end check: make_synthetic.py -> decode_sdlog.py. Run: python tools/test_sdlog.py"""
import csv, os, sys, tempfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_synthetic, decode_sdlog  # noqa: E402
from openpyxl import Workbook, load_workbook

# Split logic: 25 data rows at a cap of 10 should become All Data, All Data 2, All Data 3.
wb = Workbook()
wb.active.title = "Summary"
cols = ["t_ms", "t_s", "part", "type", "v"]
data = [[i, i / 1000.0, 0, "x", i] for i in range(25)]
ns = decode_sdlog.write_all_data_sheets(wb, cols, data, max_data_rows=10)
assert ns == 3, ns
assert wb.sheetnames == ["Summary", "All Data", "All Data 2", "All Data 3"], wb.sheetnames
assert wb["All Data"].max_row == 11          # header + 10
assert wb["All Data 2"].max_row == 11
assert wb["All Data 3"].max_row == 6         # header + 5
assert wb["All Data"].freeze_panes == "A2"
assert wb["All Data"].auto_filter.ref.startswith("A1:")
# every value made it, nothing truncated
got = []
for name in ("All Data", "All Data 2", "All Data 3"):
    for row in wb[name].iter_rows(min_row=2, values_only=True):
        got.append(row[0])
assert got == list(range(25)), got

with tempfile.TemporaryDirectory() as d:
    src, out = os.path.join(d, "sd"), os.path.join(d, "out")
    sys.argv = ["make_synthetic.py", src]
    make_synthetic.main()
    assert decode_sdlog.main([src, "-o", out]) == 0
    metas = [decode_sdlog.decode_file(os.path.join(src, f))[0] for f in sorted(os.listdir(src))]
    v2 = [m for m in metas if m["version"] == 2]
    assert len(v2) >= 2, "expected rolled parts"
    assert [m["part"] for m in v2] == list(range(len(v2)))
    assert "stale data" in v2[-1]["stop"], v2[-1]["stop"]
    assert all(m["bad"] == 0 for m in metas)
    ev = open(os.path.join(out, "boot_00007", "faults.csv")).read()
    for needle in ("IMU_STALE,ON", "IMU_STALE,cleared", "GPS_STALE,ON", "CAN_VCU_STALE,ON", "PIT_ALERT CAN_VCU_STALE", "INVERTER,raw"):
        assert needle in ev, needle
    st = open(os.path.join(out, "boot_00007", "status_decoded.csv")).read()
    assert "ABSENT" in st and "STALE" in st
    for f in ("boot_00007.xlsx", "sensors.png", "health.png", "sd_logger.png", "faults.png", "all_data.csv"):
        assert os.path.getsize(os.path.join(out, "boot_00007", f)) > 1000, f

    boot = os.path.join(out, "boot_00007")
    wb = load_workbook(os.path.join(boot, "boot_00007.xlsx"))
    assert wb.sheetnames[0] == "Summary", wb.sheetnames
    assert wb.sheetnames[1] == "All Data", wb.sheetnames
    for name in ("Faults", "Status", "car_speed", "imu", "fault_daq", "fault_inv", "pit_alert"):
        assert name in wb.sheetnames, name
    ws = wb["All Data"]
    assert ws.freeze_panes == "A2"
    assert ws.auto_filter.ref
    header = [c.value for c in ws[1]]
    assert header[:4] == ["t_ms", "t_s", "part", "type"], header[:4]
    for col in ("speed_mps", "ax_mps2", "fault_bits", "seq", "what", "state", "detail", "car_speed", "imu"):
        assert col in header, col

    # All Data rows = sum of per-record-type sheets (Status + each type except the Faults event sheet)
    per_type = 0
    for name in wb.sheetnames:
        if name in ("Summary", "Faults") or name == "All Data" or name.startswith("All Data "):
            continue
        per_type += wb[name].max_row - 1
    all_n = ws.max_row - 1
    assert all_n == per_type, "All Data %d rows vs per-type sum %d" % (all_n, per_type)

    with open(os.path.join(boot, "all_data.csv"), newline="") as f:
        csv_rows = list(csv.reader(f))
    assert csv_rows[0] == header
    assert len(csv_rows) - 1 == all_n
    t_ms = [int(r[0]) for r in csv_rows[1:]]
    assert t_ms == sorted(t_ms), "All Data is not sorted by timestamp"
    types_seen = {r[3] for r in csv_rows[1:]}
    assert "imu" in types_seen and "status" in types_seen and "fault_daq" in types_seen
    # pad records must not appear
    assert "pad" not in types_seen and "REC_PAD" not in types_seen

    what_i, state_i, type_i = header.index("what"), header.index("state"), header.index("type")
    fault_rows = [r for r in csv_rows[1:] if r[type_i] == "fault_daq"]
    assert any(r[what_i] == "IMU_STALE" and r[state_i] == "ON" for r in fault_rows), fault_rows[:5]
    pit_rows = [r for r in csv_rows[1:] if r[type_i] == "pit_alert"]
    assert any(r[what_i].startswith("PIT_ALERT") for r in pit_rows)
    inv_rows = [r for r in csv_rows[1:] if r[type_i] == "fault_inv"]
    assert any(r[what_i] == "INVERTER" and r[state_i] == "raw" and "post=" in r[header.index("detail")] for r in inv_rows)

print("OK: synthetic logs decode end to end")
