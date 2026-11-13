#!/usr/bin/env python3
"""Make synthetic FSAE DAQ SD logs that mimic the firmware byte for byte, to test
tools/decode_sdlog.py without a Teensy.

It reads the record table from include/LogConfig.h, the fault names/actions from
src/faults.cpp and the sensor list from src/DaqLog.cpp, so it stays in sync with
the firmware. It then replays the teensy41_sdbench timeline (fake IMU + wheel
speed, Brian's fault demo) through a Python copy of SDLogger: rolling parts with
a v2 header, REC_PAD padding before every 1 s sync, 1 Hz status records, fault
and pit-alert records. The last part keeps a "preallocated" tail with stale
bytes from an older log, like a real power cut, to test the decoder's end check.
A format v1 file is written too (backward compatibility).

Usage: python make_synthetic.py [outdir] [--seconds 45] [--file-bytes 65536]
"""
import argparse, math, os, re, struct

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SECTOR = 512


def read(rel):
    with open(os.path.join(ROOT, rel)) as f:
        return f.read()


def firmware_tables():
    ids = {m[0]: int(m[1]) for m in re.findall(r"(REC_\w+)\s*=\s*(\d+)", read("include/SensorSample.h"))}
    types = []
    for rec, name, fmt in re.findall(r'\{(REC_\w+),\s*sizeof\([^)]*\),\s*"(\w+)",\s*"([^"]+)"\}', read("include/LogConfig.h")):
        types.append((ids[rec], struct.calcsize(fmt), name, fmt))
    faults = re.findall(r'\{"(\w+)",\s*([A-Z_| ]+)\}', read("src/faults.cpp"))
    faults = [(n, "ACT_ALERT_PIT" in acts) for n, acts in faults]
    sensors = re.findall(r'SensorHealth::add\("(\w+)"', read("src/DaqLog.cpp"))
    return ids, types, faults, sensors


class Logger:
    """Python copy of SDLogger's on-disk behaviour."""

    def __init__(self, outdir, boot, file_bytes, types, sensors, faults, git="synthetic"):
        self.outdir, self.boot, self.cap = outdir, boot, file_bytes
        self.types, self.sensors, self.faults, self.git = types, sensors, faults, git
        self.part, self.buf, self.files = -1, bytearray(), []
        self.rotate(0)

    def header(self, start_ms):
        h = bytearray()
        body = struct.pack("<IHII", self.boot, self.part, start_ms, 0) + self.git.encode()[:15].ljust(16, b"\0") + struct.pack("<I", self.cap)
        body += struct.pack("<B", len(self.types))
        for i, ln, n, f in self.types:
            body += struct.pack("<BB", i, ln) + n.encode().ljust(16, b"\0") + f.encode().ljust(16, b"\0")
        body += struct.pack("<B", len(self.sensors)) + b"".join(s.encode()[:11].ljust(12, b"\0") for s in self.sensors)
        body += struct.pack("<B", len(self.faults)) + b"".join(n.encode()[:15].ljust(16, b"\0") for n, _ in self.faults)
        h += b"FSAEDAQ\0" + struct.pack("<HH", 2, 8 + 2 + 2 + len(body)) + body
        return h

    def flush_file(self, truncate=True):
        name = os.path.join(self.outdir, "LOG%05d_%03d.BIN" % (self.boot, self.part))
        data = bytes(self.buf)
        with open(name, "wb") as f:
            f.write(data)
        self.files.append(name)
        return name

    def rotate(self, t):
        if self.part >= 0:
            self.flush_file()
        self.part += 1
        self.buf = bytearray(self.header(t))

    def record(self, rid, ts, payload):
        if len(self.buf) + 7 + len(payload) > self.cap - SECTOR:
            return False
        self.buf += struct.pack("<BBBI", 0xA5, rid, len(payload), ts) + payload
        if len(self.buf) > self.cap - 4096:     # firmware: cap - 2 * RING_BUF_SIZE (scaled down here)
            self.rotate(ts)
        return True

    def pad(self, ts):
        need = (SECTOR - len(self.buf) % SECTOR) % SECTOR
        if need == 0:
            return
        if need < 7:
            need += SECTOR
        while need:
            chunk = min(need, 262)
            if 0 < need - chunk < 7:
                chunk = need - 7
            self.buf += struct.pack("<BBBI", 0xA5, 0, chunk - 7, ts) + bytes(chunk - 7)
            need -= chunk

    def close_with_powercut(self, stale_tail):
        # power cut: file keeps its preallocated size, tail holds bytes from an older log
        self.buf += stale_tail
        return self.flush_file()


def write_v1(path):
    types = [(1, 4, "car_speed", "<f"), (2, 24, "imu", "<6f")]
    b = b"FSAEDAQ\0" + struct.pack("<HB", 1, len(types))
    for i, ln, n, f in types:
        b += struct.pack("<BB", i, ln) + n.encode().ljust(16, b"\0") + f.encode().ljust(16, b"\0")
    for t in range(0, 1000, 10):
        b += struct.pack("<BBBI", 0xA5, 2, 24, t) + struct.pack("<6f", 0, 0, 9.81, 0, 0, 0)
        if t % 20 == 0:
            b += struct.pack("<BBBI", 0xA5, 1, 4, t) + struct.pack("<f", t / 100)
    b += bytes(4096)
    with open(path, "wb") as f:
        f.write(b)


def simulate(outdir, seconds, file_bytes, boot=7):
    ids, types, faults, sensors = firmware_tables()
    fault_idx = {n: i for i, (n, _) in enumerate(faults)}
    lg = Logger(outdir, boot, file_bytes, types, sensors, faults)
    timeouts = {"car_speed": 100, "imu": 50, "brake_temp": 500, "shock_pot": 50, "steering": 50, "gps": 3000, "lora": 0}
    hotplug = {"imu", "lora"}
    last = {s: 0 for s in sensors}
    present = {s: s not in hotplug for s in sensors}
    present_since = {s: 0 for s in sensors}
    last_probe = {s: -10 ** 9 for s in sensors}
    imu_on, lora_on, wheel_on = True, False, True
    # Brian's fault tracker state
    stale_cnt = [0] * len(faults)
    fault_bits = handled = 0
    last_check = 0
    lastf = {"ADC": 0, "TC": 0, "IMU": 0, "GPS": 0, "CAN_INV": 0, "CAN_BMS": 0, "CAN_VCU": 0}
    ftimeout = {"ADC": 50, "TC": 500, "IMU": 50, "GPS": 3000, "CAN_INV": 70, "CAN_BMS": 70, "CAN_VCU": 70}
    inv_last = None
    seq = 0
    dropped = 0
    max_used = 0
    for t in range(0, seconds * 1000):
        # bench script
        if t == 10000: imu_on = False
        if t == 20000: imu_on = True
        if t == 25000: lora_on = True
        if t == 30000: wheel_on = False
        if t == 35000: wheel_on = True
        # health: probe / poll (SensorHealth::service)
        for s in sensors:
            if s in hotplug and not present[s] and t - last_probe[s] >= 1000:
                last_probe[s] = t
                if (s == "imu" and imu_on) or (s == "lora" and lora_on):
                    present[s], present_since[s] = True, t
        if present["imu"] and imu_on and t % 10 == 0:
            last["imu"] = t
            sec = t / 1000
            lg.record(ids["REC_IMU"], t, struct.pack("<6f", 0.3 * math.sin(sec), 0.8 * math.sin(0.5 * sec), 9.81, 0, 0, 0.2 * math.sin(0.5 * sec)))
            lastf["IMU"] = t
        if wheel_on and t % 20 == 0:
            last["car_speed"] = t
            lg.record(ids["REC_CAR_SPEED"], t, struct.pack("<f", 12 + 3 * math.sin(t * 0.0005)))
        # Brian's demo timeline (fault_demo.cpp)
        if t < 15000 or t >= 19000: lastf["CAN_VCU"] = t
        if t < 17000 or t >= 21000: lastf["ADC"] = t
        lastf["TC"] = lastf["CAN_INV"] = lastf["CAN_BMS"] = t
        inv = (0, 0, 2 if 5000 <= t < 6000 else 0, 0)
        if inv != inv_last:
            lg.record(ids["REC_FAULT_INV"], t, struct.pack("<4H", *inv))
            inv_last = inv
        # alive / unplug (after poll)
        alive = {}
        for s in sensors:
            if not present[s]:
                alive[s] = False
                continue
            if timeouts[s] == 0:
                alive[s] = True
                continue
            alive[s] = last[s] != 0 and t - last[s] <= timeouts[s]
            if s in hotplug and not alive[s]:
                quiet = last[s] if last[s] > present_since[s] else present_since[s]
                if t - quiet > 5000:
                    present[s], last_probe[s] = False, t
        # 1 Hz status
        if t % 1000 == 0 and t > 0:
            pm = sum(1 << i for i, s in enumerate(sensors) if present[s])
            am = sum(1 << i for i, s in enumerate(sensors) if alive[s])
            used = 300 + (seq * 37) % 900
            max_used = max(max_used, used, 2048 if t >= 2000 else 0)
            st = struct.pack("<3H8IHII", seq, pm, am, 3712 - lg.part * 64, 0, dropped, 0, max_used, used, 0,
                             fault_bits, lg.part, len(lg.buf), 950 if lg.part else 420)
            lg.record(ids["REC_STATUS"], t, st)
            seq += 1
        # Brian's checkFaults()
        if t >= 2000 and t - last_check >= 10:
            last_check = t
            for key in ("ADC", "TC", "IMU", "GPS", "CAN_INV", "CAN_BMS", "CAN_VCU"):
                f = fault_idx[key + "_STALE"]
                if t - lastf[key] > ftimeout[key]:
                    stale_cnt[f] = min(stale_cnt[f] + 1, 3)
                    if stale_cnt[f] >= 3: fault_bits |= 1 << f
                else:
                    stale_cnt[f] = 0
                    fault_bits &= ~(1 << f)
            changed = fault_bits ^ handled
            if changed:
                for i, (name, pit) in enumerate(faults):
                    if changed & (1 << i) and pit:
                        lg.record(ids["REC_PIT_ALERT"], t, struct.pack("<3B", i, 1 if fault_bits & (1 << i) else 0, 1 if present["lora"] else 0))
                lg.record(ids["REC_FAULT_DAQ"], t, struct.pack("<I", fault_bits))
                handled = fault_bits
        # 1 s padded sync
        if t % 1000 == 999:
            lg.pad(t)
    # power cut at the end: last part keeps a preallocated tail with an older log's bytes
    stale = bytearray()
    stale += struct.pack("<BBBI", 0xA5, ids["REC_CAR_SPEED"], 4, 987654321) + struct.pack("<f", 99.0)   # aligned junk
    stale += os.urandom(2048)
    lg.close_with_powercut(bytes(stale))
    return lg.files


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("outdir", nargs="?", default="/tmp/fsae_synth")
    ap.add_argument("--seconds", type=int, default=45)
    ap.add_argument("--file-bytes", type=int, default=64 * 1024, help="part size cap (small so it rotates)")
    a = ap.parse_args()
    os.makedirs(a.outdir, exist_ok=True)
    for f in os.listdir(a.outdir):
        if f.upper().startswith("LOG") and f.upper().endswith(".BIN"):
            os.remove(os.path.join(a.outdir, f))
    write_v1(os.path.join(a.outdir, "LOG00006.BIN"))
    files = simulate(a.outdir, a.seconds, a.file_bytes)
    print("wrote LOG00006.BIN (format v1) and %d rolled parts for boot 7 in %s" % (len(files), a.outdir))


if __name__ == "__main__":
    main()
