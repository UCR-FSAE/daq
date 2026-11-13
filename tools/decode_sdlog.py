#!/usr/bin/env python3
"""Decode FSAE DAQ SD logs (format v1 and v2) into CSV, an Excel workbook and PNG plots.

Usage:
  python decode_sdlog.py LOG00007_000.BIN [more files...] [-o outdir]
  python decode_sdlog.py /path/to/sdcard/            (every LOG*.BIN in the folder)
  options: --no-plots  --no-excel  --boot N

v2 files are rolling parts (LOGbbbbb_ppp.BIN). Parts of the same boot are stitched
together in part order, so one boot = one set of outputs:
  outdir/boot_bbbbb/<record>.csv      every record type, named columns
  outdir/boot_bbbbb/faults.csv        fault ON/cleared events named from the header
  outdir/boot_bbbbb/all_data.csv      every decoded record in one time-sorted table
  outdir/boot_bbbbb/boot_bbbbb.xlsx   Summary, All Data (every record, time-sorted),
                                      Faults, Status and one sheet per record
  outdir/boot_bbbbb/*.png             speed/IMU, sensor health, SD buffer, fault timeline

The All Data sheet (and all_data.csv) has one row per decoded record across every
record type: sensors, IMU, status, faults, and anything else in the log. Pad records
are skipped. Columns are t_ms, t_s, part, type, then the union of every field across
all types (blank if that field does not apply to the row). Fault rows also carry
readable what/state/detail. If the table is larger than Excel's 1,048,576-row cap,
it continues on "All Data 2", "All Data 3", and so on; all_data.csv always has
everything.

Needs: matplotlib, openpyxl (pip install matplotlib openpyxl) for plots / Excel.
"""
import argparse, csv, glob, os, re, struct, sys
from collections import defaultdict

MAGIC = b"FSAEDAQ\0"
REC_PAD = 0
# Excel's hard cap is 1,048,576 rows including the header. Per-type sheets still
# truncate a little earlier; All Data splits onto extra sheets instead.
EXCEL_MAX_ROWS = 1_000_000
EXCEL_SHEET_ROWS = 1_048_576
EXCEL_ALL_DATA_MAX_ROWS = EXCEL_SHEET_ROWS - 1

# Column names for known record types (falls back to v0, v1, ...).
FIELDS = {
    "car_speed": ["speed_mps"],
    "imu": ["ax_mps2", "ay_mps2", "az_mps2", "gx_dps", "gy_dps", "gz_dps"],
    "brake_temp": ["fl_C", "fr_C", "rl_C", "rr_C"],
    "shock_pot": ["fl_mm", "fr_mm", "rl_mm", "rr_mm"],
    "steering": ["angle_deg"],
    "fault_daq": ["fault_bits"],
    "fault_inv": ["post_lo", "post_hi", "run_lo", "run_hi"],
    "pit_alert": ["fault", "active", "sent_lora"],
    "status": ["seq", "present_mask", "alive_mask", "sd_free_mb", "write_errors", "dropped_bytes",
               "dropped_records", "buf_max_used", "buf_used", "overruns", "faults", "file_part",
               "file_bytes", "max_service_us"],
}

# Plausibility limits for finding the real end of data in a preallocated part.
MAX_BACKWARD_MS = 2000      # sensor timestamps can be a little older than the previous record
MAX_FORWARD_MS = 600_000    # a 10 min gap inside one part means we're reading stale bytes


def cstr(b):
    return b.split(b"\0")[0].decode(errors="replace")


def parse_header(b, path):
    if b[:8] != MAGIC:
        raise ValueError("%s: bad magic (not an FSAE DAQ log)" % path)
    ver = struct.unpack_from("<H", b, 8)[0]
    meta = {"file": os.path.basename(path), "version": ver, "sensors": [], "faults": []}
    if ver == 1:
        n = b[10]
        off = 11
        m = re.search(r"LOG(\d+)", os.path.basename(path), re.I)
        meta.update(boot=int(m.group(1)) if m else 0, part=0, start_ms=0, rtc_unix=0, git="", file_cap=0)
    elif ver == 2:
        hdr_len, boot, part, start_ms, rtc = struct.unpack_from("<HIHII", b, 10)
        git = cstr(b[26:42])
        cap = struct.unpack_from("<I", b, 42)[0]
        meta.update(boot=boot, part=part, start_ms=start_ms, rtc_unix=rtc, git=git, file_cap=cap, header_len=hdr_len)
        n = b[46]
        off = 47
    else:
        raise ValueError("%s: unknown format version %d" % (path, ver))
    types = {}
    for _ in range(n):
        tid, ln = struct.unpack_from("<BB", b, off)
        types[tid] = (ln, cstr(b[off + 2:off + 18]), cstr(b[off + 18:off + 34]))
        off += 34
    if ver == 2:
        ns = b[off]; off += 1
        meta["sensors"] = [cstr(b[off + 12 * i:off + 12 * i + 12]) for i in range(ns)]; off += 12 * ns
        nf = b[off]; off += 1
        meta["faults"] = [cstr(b[off + 16 * i:off + 16 * i + 16]) for i in range(nf)]; off += 16 * nf
        off = meta["header_len"]
    return meta, types, off


def decode_file(path):
    b = open(path, "rb").read()
    meta, types, off = parse_header(b, path)
    rows = defaultdict(list)
    pads = bad = 0
    last_ts = None
    stop = "end of file"
    while off + 7 <= len(b):
        if b[off] != 0xA5:
            stop = "no record marker (unused preallocated space)"
            break
        tid, ln, ts = struct.unpack_from("<BBI", b, off + 1)
        if off + 7 + ln > len(b):
            stop = "record runs past end of file"
            break
        if tid == REC_PAD:
            pads += 1
            off += 7 + ln
            continue
        known = tid in types and types[tid][0] == ln
        if meta["version"] >= 2:
            # In v2 an unknown id or a wild timestamp means stale bytes from an older log.
            if not known:
                stop = "unknown record id %d / length %d (stale data)" % (tid, ln)
                break
            if last_ts is not None and (ts + MAX_BACKWARD_MS < last_ts or ts > last_ts + MAX_FORWARD_MS):
                stop = "timestamp jump %d -> %d ms (stale data)" % (last_ts, ts)
                break
        if known:
            rows[types[tid][1]].append((ts,) + struct.unpack(types[tid][2], b[off + 7:off + 7 + ln]))
            last_ts = ts
        else:
            bad += 1
        off += 7 + ln
    meta.update(pads=pads, bad=bad, data_end=off, size=len(b), unused_bytes=len(b) - off, stop=stop,
                records=sum(len(v) for v in rows.values()))
    return meta, types, rows


def collect(paths):
    files = []
    for p in paths:
        if os.path.isdir(p):
            files += [f for f in glob.glob(os.path.join(p, "*")) if re.match(r"LOG.*\.BIN$", os.path.basename(f), re.I)]
        else:
            files.append(p)
    return files


def columns(name, fmt, types_len):
    k = len(struct.unpack(fmt, bytes(struct.calcsize(fmt))))
    names = FIELDS.get(name, [])
    return ["t_ms"] + (names if len(names) == k else ["v%d" % i for i in range(k)])


def fault_events(rows, fault_names):
    """DAQ fault bitmask records -> one row per fault that turned ON or cleared."""
    ev, prev = [], 0
    for r in rows.get("fault_daq", []):
        t, bits = r[0], r[1]
        ch = bits ^ prev
        for i in range(32):
            if ch & (1 << i):
                name = fault_names[i] if i < len(fault_names) else "FAULT_%d" % i
                ev.append((t, name, "ON" if bits & (1 << i) else "cleared", "0x%08X" % bits))
        prev = bits
    for r in rows.get("fault_inv", []):
        post = (r[2] << 16) | r[1]
        run = (r[4] << 16) | r[3]
        ev.append((r[0], "INVERTER", "raw", "post=0x%08X run=0x%08X" % (post, run)))
    for r in rows.get("pit_alert", []):
        name = fault_names[r[1]] if r[1] < len(fault_names) else "FAULT_%d" % r[1]
        ev.append((r[0], "PIT_ALERT " + name, "ON" if r[2] else "cleared", "sent over LoRa" if r[3] else "LoRa absent, SD only"))
    ev.sort(key=lambda e: e[0])
    return ev


def status_table(rows, sensors):
    cols = FIELDS["status"]
    out = []
    for r in rows.get("status", []):
        d = dict(zip(["t_ms"] + cols, r))
        row = [d["t_ms"]] + [d[c] for c in cols]
        for i, s in enumerate(sensors):
            p, a = d["present_mask"] >> i & 1, d["alive_mask"] >> i & 1
            row.append("OK" if a else ("STALE" if p else "ABSENT"))
        out.append(row)
    return ["t_ms"] + cols + sensors, out


def all_data_columns(types, sensors):
    """t_ms, t_s, part, type, then the union of every field across all record types."""
    cols = ["t_ms", "t_s", "part", "type"]
    seen = set(cols)

    def add(name):
        if name not in seen:
            cols.append(name)
            seen.add(name)

    for tid, (ln, name, fmt) in sorted(types.items()):
        for c in columns(name, fmt, ln)[1:]:
            add(c)
    for s in sensors:
        add(s)
    for c in ("what", "state", "detail"):
        add(c)
    return cols


def _fault_readable(name, rec, fault_names, prev_bits):
    """Readable what/state/detail for one fault_* record. Returns (what, state, detail, new_prev)."""
    if name == "fault_daq":
        bits = rec[1]
        ch = bits ^ prev_bits
        changes = []
        for i in range(32):
            if ch & (1 << i):
                fname = fault_names[i] if i < len(fault_names) else "FAULT_%d" % i
                changes.append((fname, "ON" if bits & (1 << i) else "cleared"))
        what = ", ".join(c[0] for c in changes)
        state = ", ".join(c[1] for c in changes) if changes else ("ON" if bits else "cleared")
        return what, state, "0x%08X" % bits, bits
    if name == "fault_inv":
        post = (rec[2] << 16) | rec[1]
        run = (rec[4] << 16) | rec[3]
        return "INVERTER", "raw", "post=0x%08X run=0x%08X" % (post, run), prev_bits
    if name == "pit_alert":
        fname = fault_names[rec[1]] if rec[1] < len(fault_names) else "FAULT_%d" % rec[1]
        detail = "sent over LoRa" if rec[3] else "LoRa absent, SD only"
        return "PIT_ALERT " + fname, "ON" if rec[2] else "cleared", detail, prev_bits
    return "", "", "", prev_bits


def build_all_data(parts, types, sensors, fault_names):
    """One time-sorted row per decoded record (pads already dropped in decode_file).

    parts is a list of (meta, typemap, rows) in part order. Each row is
    [t_ms, t_s, part, type, ...union fields...]. Blank cells are "".
    """
    cols = all_data_columns(types, sensors)
    recs = []
    for meta, typemap, rows in parts:
        part = meta.get("part")
        for tid, (ln, name, fmt) in typemap.items():
            names = columns(name, fmt, ln)
            for rec in rows.get(name, []):
                d = dict(zip(names, rec))
                d["part"] = part
                d["type"] = name
                recs.append((d, rec))
    recs.sort(key=lambda item: (item[0]["t_ms"], item[0].get("part") or 0, item[0]["type"]))

    prev_bits = 0
    out = []
    for d, rec in recs:
        d["t_s"] = d["t_ms"] / 1000.0
        name = d["type"]
        if name == "status":
            pm, am = d.get("present_mask", 0), d.get("alive_mask", 0)
            for i, s in enumerate(sensors):
                p, a = pm >> i & 1, am >> i & 1
                d[s] = "OK" if a else ("STALE" if p else "ABSENT")
        elif name in ("fault_daq", "fault_inv", "pit_alert"):
            what, state, detail, prev_bits = _fault_readable(name, rec, fault_names, prev_bits)
            d["what"], d["state"], d["detail"] = what, state, detail
        out.append([d[c] if c in d else "" for c in cols])
    return cols, out


def write_all_data_csv(path, cols, rows):
    with open(path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(cols)
        w.writerows(rows)


def write_all_data_sheets(wb, cols, data, max_data_rows=EXCEL_ALL_DATA_MAX_ROWS):
    """Write All Data, then All Data 2, All Data 3, ... if the table exceeds max_data_rows.

    Sheets are inserted immediately after Summary (index 1, 2, ...). Header is frozen
    and autofilter is on. max_data_rows is overridable so the split can be unit-tested
    without building a million-row workbook.
    """
    from openpyxl.styles import Font
    from openpyxl.utils import get_column_letter

    if max_data_rows < 1:
        raise ValueError("max_data_rows must be at least 1")
    n = len(data)
    nsheets = max(1, (n + max_data_rows - 1) // max_data_rows)
    last_col = get_column_letter(len(cols))
    for i in range(nsheets):
        chunk = data[i * max_data_rows:(i + 1) * max_data_rows]
        title = "All Data" if i == 0 else "All Data %d" % (i + 1)
        s = wb.create_sheet(title[:31], 1 + i)
        s.append(cols)
        for c in s[1]:
            c.font = Font(bold=True)
        for r in chunk:
            s.append([None if v == "" else v for v in r])
        s.freeze_panes = "A2"
        s.auto_filter.ref = "A1:%s%d" % (last_col, s.max_row)
    return nsheets


def write_csvs(outdir, merged, types, events, st_cols, st_rows):
    for tid, (ln, name, fmt) in sorted(types.items()):
        with open(os.path.join(outdir, name + ".csv"), "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(columns(name, fmt, ln))
            w.writerows(merged.get(name, []))
    with open(os.path.join(outdir, "faults.csv"), "w", newline="") as f:
        w = csv.writer(f); w.writerow(["t_ms", "what", "state", "detail"]); w.writerows(events)
    if st_rows:
        with open(os.path.join(outdir, "status_decoded.csv"), "w", newline="") as f:
            w = csv.writer(f); w.writerow(st_cols); w.writerows(st_rows)


def write_excel(path, metas, merged, types, events, st_cols, st_rows, all_cols=None, all_rows=None,
                max_data_rows=EXCEL_ALL_DATA_MAX_ROWS):
    from openpyxl import Workbook
    from openpyxl.styles import Font
    wb = Workbook()
    ws = wb.active
    ws.title = "Summary"
    m0 = metas[0]
    ws.append(["FSAE DAQ SD log", "boot %d" % m0["boot"]])
    ws["A1"].font = Font(bold=True)
    ws.append(["format version", m0["version"]])
    ws.append(["git", m0.get("git", "")])
    ws.append(["part cap (bytes)", m0.get("file_cap", 0)])
    ws.append(["sensors (status bit order)", ", ".join(m0["sensors"])])
    ws.append([])
    hdr = ["file", "part", "start_ms", "records", "pad records", "bad", "data bytes", "unused bytes", "end of data"]
    ws.append(hdr)
    for c in ws[ws.max_row]:
        c.font = Font(bold=True)
    for m in metas:
        ws.append([m["file"], m["part"], m["start_ms"], m["records"], m["pads"], m["bad"], m["data_end"], m["unused_bytes"], m["stop"]])
    ws.append([])
    ws.append(["record", "rows"])
    for tid, (ln, name, fmt) in sorted(types.items()):
        ws.append([name, len(merged.get(name, []))])
    ws.column_dimensions["A"].width = 26
    ws.column_dimensions["I"].width = 48

    if all_cols is not None:
        write_all_data_sheets(wb, all_cols, all_rows or [], max_data_rows=max_data_rows)

    def sheet(title, cols, data):
        s = wb.create_sheet(title[:31])
        s.append(cols)
        for c in s[1]:
            c.font = Font(bold=True)
        for r in data[:EXCEL_MAX_ROWS]:
            s.append(list(r))
        if len(data) > EXCEL_MAX_ROWS:
            s.append(["truncated at %d rows, see CSV" % EXCEL_MAX_ROWS])
        s.freeze_panes = "A2"

    sheet("Faults", ["t_ms", "what", "state", "detail"], events)
    if st_rows:
        sheet("Status", st_cols, st_rows)
    for tid, (ln, name, fmt) in sorted(types.items()):
        if name in ("status",):
            continue
        sheet(name, columns(name, fmt, ln), merged.get(name, []))
    wb.save(path)


def write_plots(outdir, merged, sensors, fault_names, part_starts):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    made = []

    def vlines(ax):
        for t in part_starts[1:]:
            ax.axvline(t / 1000, color="0.6", ls=":", lw=0.8)

    # 1) speed + IMU
    fig, axs = plt.subplots(2, 1, figsize=(10, 6), sharex=True)
    sp = merged.get("car_speed", [])
    if sp:
        axs[0].plot([r[0] / 1000 for r in sp], [r[1] for r in sp], ".", ms=2)
    axs[0].set_ylabel("car speed (m/s)")
    imu = merged.get("imu", [])
    for i, lbl in enumerate(["ax", "ay", "az"]):
        if imu:
            axs[1].plot([r[0] / 1000 for r in imu], [r[1 + i] for r in imu], ".", ms=1.5, label=lbl)
    axs[1].set_ylabel("accel (m/s^2)"); axs[1].set_xlabel("time since boot (s)"); axs[1].legend(loc="upper right")
    for a in axs:
        vlines(a); a.grid(alpha=0.3)
    fig.suptitle("Sensors (dotted lines = new log part)")
    fig.tight_layout(); p = os.path.join(outdir, "sensors.png"); fig.savefig(p, dpi=110); plt.close(fig); made.append(p)

    st = merged.get("status", [])
    if st:
        t = [r[0] / 1000 for r in st]
        # 2) health raster
        fig, ax = plt.subplots(figsize=(10, 0.45 * len(sensors) + 1.5))
        colors = {"OK": "tab:green", "STALE": "tab:orange", "ABSENT": "0.75"}
        for i, s in enumerate(sensors):
            for r in st:
                p_, a_ = r[2] >> i & 1, r[3] >> i & 1
                state = "OK" if a_ else ("STALE" if p_ else "ABSENT")
                ax.barh(i, 1.0, left=r[0] / 1000 - 0.5, color=colors[state], edgecolor="none")
        ax.set_yticks(range(len(sensors))); ax.set_yticklabels(sensors); ax.invert_yaxis()
        ax.set_xlabel("time since boot (s)")
        ax.set_title("Sensor health from 1 Hz status (green OK, orange STALE, grey ABSENT)")
        fig.tight_layout(); p = os.path.join(outdir, "health.png"); fig.savefig(p, dpi=110); plt.close(fig); made.append(p)
        # 3) SD logger
        fig, axs = plt.subplots(3, 1, figsize=(10, 7), sharex=True)
        axs[0].plot(t, [r[9] for r in st], label="buffer used"); axs[0].plot(t, [r[8] for r in st], label="buffer max used")
        axs[0].axhline(65536, color="r", ls="--", lw=0.8, label="64 KiB buffer"); axs[0].set_ylabel("bytes"); axs[0].legend(loc="upper left")
        axs[1].plot(t, [r[6] for r in st], label="dropped bytes"); axs[1].plot(t, [r[5] for r in st], label="write errors")
        axs[1].plot(t, [r[10] for r in st], label="overruns"); axs[1].legend(loc="upper left"); axs[1].set_ylabel("count")
        free = [r[4] if r[4] != 0xFFFFFFFF else float("nan") for r in st]
        axs[2].plot(t, free, label="SD free (MB)"); axs[2].set_ylabel("MB")
        ax2 = axs[2].twinx(); ax2.step(t, [r[12] for r in st], color="tab:purple", where="post"); ax2.set_ylabel("part", color="tab:purple")
        axs[2].set_xlabel("time since boot (s)")
        for a in axs:
            vlines(a); a.grid(alpha=0.3)
        fig.suptitle("SD logger health")
        fig.tight_layout(); p = os.path.join(outdir, "sd_logger.png"); fig.savefig(p, dpi=110); plt.close(fig); made.append(p)

    # 4) fault timeline
    fd = merged.get("fault_daq", [])
    if fd:
        end = max(r[0] for v in merged.values() for r in v) if merged else 0
        used = sorted({i for r in fd for i in range(32) if r[1] >> i & 1})
        fig, ax = plt.subplots(figsize=(10, 0.45 * max(len(used), 1) + 1.5))
        for row, i in enumerate(used):
            on = None
            seq = fd + [(end, 0)]
            for r in seq:
                active = r[1] >> i & 1
                if active and on is None:
                    on = r[0]
                elif not active and on is not None:
                    ax.barh(row, (r[0] - on) / 1000, left=on / 1000, color="tab:red")
                    on = None
        ax.set_yticks(range(len(used)))
        ax.set_yticklabels([fault_names[i] if i < len(fault_names) else "bit %d" % i for i in used]); ax.invert_yaxis()
        for r in merged.get("fault_inv", []):
            if r[3] or r[1] or r[2] or r[4]:
                ax.axvline(r[0] / 1000, color="k", ls="--", lw=0.8)
        ax.set_xlabel("time since boot (s)"); ax.set_title("DAQ faults (red = ON, dashed = inverter fault word non-zero)")
        ax.grid(alpha=0.3, axis="x")
        fig.tight_layout(); p = os.path.join(outdir, "faults.png"); fig.savefig(p, dpi=110); plt.close(fig); made.append(p)
    return made


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("paths", nargs="+")
    ap.add_argument("-o", "--outdir", default="decoded")
    ap.add_argument("--no-plots", action="store_true")
    ap.add_argument("--no-excel", action="store_true")
    ap.add_argument("--boot", type=int, help="only this boot number")
    a = ap.parse_args(argv)

    by_boot = defaultdict(list)
    for f in collect(a.paths):
        try:
            meta, types, rows = decode_file(f)
        except ValueError as e:
            print("skip:", e)
            continue
        by_boot[meta["boot"]].append((meta, types, rows))
    if not by_boot:
        print("no logs found"); return 1

    for boot in sorted(by_boot):
        if a.boot is not None and boot != a.boot:
            continue
        parts = sorted(by_boot[boot], key=lambda x: x[0]["part"])
        metas = [p[0] for p in parts]
        types = {}
        for p in parts:
            types.update(p[1])
        merged = defaultdict(list)
        for _, _, rows in parts:
            for k, v in rows.items():
                merged[k] += v
        sensors, fault_names = metas[0]["sensors"], metas[0]["faults"]
        nums = [m["part"] for m in metas]
        gaps = sorted(set(range(min(nums), max(nums) + 1)) - set(nums))
        out = os.path.join(a.outdir, "boot_%05d" % boot)
        os.makedirs(out, exist_ok=True)
        events = fault_events(merged, fault_names)
        st_cols, st_rows = status_table(merged, sensors)
        all_cols, all_rows = build_all_data(parts, types, sensors, fault_names)
        write_csvs(out, merged, types, events, st_cols, st_rows)
        write_all_data_csv(os.path.join(out, "all_data.csv"), all_cols, all_rows)
        print("boot %d: format v%d, %d part(s)%s, git %s" % (boot, metas[0]["version"], len(parts),
              (" MISSING parts %s" % gaps) if gaps else "", metas[0].get("git") or "-"))
        for m in metas:
            print("  %-18s %7d records %4d pads, %6d unused bytes, stopped: %s" % (m["file"], m["records"], m["pads"], m["unused_bytes"], m["stop"]))
        for tid, (ln, name, fmt) in sorted(types.items()):
            print("  %-11s %7d rows" % (name, len(merged.get(name, []))))
        if events:
            print("  fault events: %d (first: %s)" % (len(events), ", ".join("%s %s@%dms" % (e[1], e[2], e[0]) for e in events[:3])))
        print("  all data:     %7d rows -> all_data.csv" % len(all_rows))
        if not a.no_excel:
            try:
                x = os.path.join(out, "boot_%05d.xlsx" % boot)
                write_excel(x, metas, merged, types, events, st_cols, st_rows, all_cols, all_rows)
                print("  excel:", x)
            except ImportError:
                print("  (openpyxl not installed, skipping Excel)")
        if not a.no_plots:
            try:
                for p in write_plots(out, merged, sensors, fault_names, [m["start_ms"] for m in metas]):
                    print("  plot:", p)
            except ImportError:
                print("  (matplotlib not installed, skipping plots)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
