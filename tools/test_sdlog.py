#!/usr/bin/env python3
"""End-to-end check: make_synthetic.py -> decode_sdlog.py. Run: python tools/test_sdlog.py"""
import os, sys, tempfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_synthetic, decode_sdlog  # noqa: E402

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
    for f in ("boot_00007.xlsx", "sensors.png", "health.png", "sd_logger.png", "faults.png"):
        assert os.path.getsize(os.path.join(out, "boot_00007", f)) > 1000, f
print("OK: synthetic logs decode end to end")
