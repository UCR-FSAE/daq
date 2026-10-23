#!/usr/bin/env python3
"""Decode FSAE DAQ SD log (format v1) -> one CSV per record type.
Usage: python decode_sdlog.py LOG00000.BIN [outdir]"""
import struct, sys, os, csv

def decode(path):
    b = open(path, "rb").read()
    if b[:8] != b"FSAEDAQ\0": raise ValueError("bad magic")
    ver, n = struct.unpack_from("<HB", b, 8); off = 11
    types = {}
    for _ in range(n):
        tid, ln = struct.unpack_from("<BB", b, off)
        name = b[off+2:off+18].split(b"\0")[0].decode()
        fmt = b[off+18:off+34].split(b"\0")[0].decode()
        types[tid] = (ln, name, fmt); off += 34
    rows = {t: [] for t in types}; bad = 0
    while off + 7 <= len(b):
        if b[off] != 0xA5: break          # end of data (preallocated/zero area)
        tid, ln, ts = struct.unpack_from("<BBI", b, off + 1)
        if off + 7 + ln > len(b): break
        p = b[off+7:off+7+ln]
        if tid in types and types[tid][0] == ln:
            rows[tid].append((ts,) + struct.unpack(types[tid][2], p))
        else: bad += 1
        off += 7 + ln
    return ver, types, rows, bad

if __name__ == "__main__":
    ver, types, rows, bad = decode(sys.argv[1])
    out = sys.argv[2] if len(sys.argv) > 2 else "."
    os.makedirs(out, exist_ok=True)
    for t, (ln, name, fmt) in types.items():
        with open(os.path.join(out, f"{name}.csv"), "w", newline="") as f:
            w = csv.writer(f); k = len(struct.unpack(fmt, bytes(ln)))
            w.writerow(["t_ms"] + [f"v{i}" for i in range(k)]); w.writerows(rows[t])
        print(f"{name}: {len(rows[t])} records")
    print(f"format v{ver}, unknown/bad records: {bad}")
