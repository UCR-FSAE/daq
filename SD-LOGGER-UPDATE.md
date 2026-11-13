# SD logger v3: sensors, fault tracking and SD logging

Branch `jaden/sd-logger-v3` (local, not pushed). Builds on `jaden/sd-logger` + Jaden's v2 code + `Brian/fault-tracking`.

## How it fits together
- **Sensors** each fill one `SensorSample<T>` slot (`value`, `timestampMs = current_time_ms`, `newSample`). One `.h` per peripheral; drivers never touch the SD card.
- **main.cpp** is the same as `main` except: `sdLoggerBegin()` in `setup()`, `logSensorsToSD()` under `// write to sd card`, and Brian's `checkFaults(current_time_ms)` at the end of `loop()`.
- **`logSensorsToSD()`** (`src/DaqLog.cpp`) runs once per loop: probes and polls hot-plug sensors (`SensorHealth`), logs every slot whose flag is set, writes a **1 Hz status record**, updates the SD faults, then lets `SDLogger::service()` write at most one 512 B sector.
- **Health:** each sensor is OK (fresh data), STALE (plugged in, no fresh data) or ABSENT. The IMU (ICM-20948 WHO_AM_I on Wire2) and LoRa are hot-plug: while absent they are probed once a second. Once detected they are polled every loop. After 5 s with no data they count as unplugged again.
- **Fault tracking** (Brian) keeps its bitmask and actions. Its log hook now calls `SDLogger::writeRecord()`, the same path as sensor data. Records: DAQ fault bitmask on every change (id 20), raw inverter CAN 0x0AB (21), and pit alerts with "sent over LoRa?" (22). SD problems become `SD_INIT/OPEN/WRITE_FAIL` faults with a 1 s retry, and there is a new `SD_FULL` fault.
- **SD logger** (`src/SDLogger.cpp`): SdFat on the built-in SDIO slot, a 64 KB RingBuf, and 512 B writes only when the card is not busy. Each boot gets a new boot number. It writes rolling `LOGbbbbb_ppp.BIN` parts of 64 MiB, starting a new part on size or after 15 min. Only one part is preallocated at boot, and the spare is preallocated later while the buffer is nearly empty. Every second it pads the open sector, writes it and syncs, so a power cut loses at most about 1 s. Files are opened with `O_EXCL`, so nothing is ever overwritten.
- **Deleting the oldest part when space is low is OFF by default** (`-D SDLOG_DELETE_OLDEST=1` turns it on). This follows the "never overwrite" rule. When the card is full, logging stops and `SD_FULL` is raised.
- **Self-describing header (v2):** format version, boot/part, start ms, RTC time, git hash (`tools/pio_git_hash.py`), part size, the record table (name + Python struct format), the sensor names (status bit order) and the fault names (fault bit order).
- **Status record (id 250, 1 Hz):** present/alive masks, SD free MB, write errors, dropped bytes and records, RingBuf `maxUsed` and current use, overruns, fault bitmask, part number, bytes in the part, and the longest sector write. The same line is printed on Serial.

## What was tested (on the box, no Teensy)
- `pio run -e teensy41` and `-e teensy41_sdbench`: both **SUCCESS**, no warnings in our files. teensy41 uses FLASH code 71.6 KB, RAM1 variables 78.5 KB (347 KB free for locals). Struct sizes are checked against the decoder formats with `static_assert`.
- `tools/make_synthetic.py` reads the record table, fault list and sensor list from the firmware sources. It replays the bench timeline (IMU unplugged at 10 to 20 s, LoRa plugged in at 25 s, wheel speed off at 30 to 35 s, Brian's inverter/VCU/ADC faults) through a Python copy of the logger. Its output is 3 rolled parts with padding, status and faults, a power-cut tail of stale bytes, and one v1 file.
- `tools/decode_sdlog.py` decoded everything: it stitched the parts in order, stopped at the stale tail and found no bad records. It wrote CSVs, an Excel workbook and 4 plots: `docs/sd-logger/{sensors,health,sd_logger,faults}.png`. `tools/test_sdlog.py` runs this end to end with assertions and passes.

## What's left
1. **Real Teensy bench test** (`pio run -e teensy41_sdbench -t upload`, 4 MiB parts). Check the "SD startup N ms" line (target well under 1 s; the old 1 GiB preallocation took ~6 s), rotation, `prealloc=` time, `max`/`drop` in STATUS, and the bench keys `i w g l v`. Pull power mid-run and confirm at most ~1 s is lost. Test FAT32 (4 GB) and exFAT cards.
2. **GPS:** the driver is not wired yet. Health uses Brian's `last_gps_ms`, so GPS shows STALE and `GPS_STALE` stays on until it is. Use the SIM output for GPS-less testing.
3. **Car test:** real data rates, sensor timeouts (Brian's placeholders), long-run buffer high-water mark, and card stalls.
4. Wire in the real drivers: WILL/IMU into `imuProbe/imuPoll`, and the telemetry module into `loraProbe/loraSendAlert`. The LoRa probe is a stub until the radio handshake is known.

## Notes for Brian
- Merge conflict was only in `main.cpp`. The DEMO handlers and timeline moved to `src/bench/fault_demo.cpp` (bench env only), so main stays clean. `demoSdLog` was replaced by the real SD hook.
- New `FAULT_SD_FULL` (proposed SD-04) was appended before `FAULT_COUNT`, so existing bits are unchanged. Please add it to the Fault Sheet.
- `faults.h` says IMU WHO_AM_I is reg 0Fh = 0x6B, but WILL/IMU uses an ICM-20948 (reg 00h = 0xEA). Please update the comment and the sheet.
- `last_*_ms` duplicates each `SensorSample.timestampMs`. For now `logSensorsToSD()` copies `imuSample`'s time into `last_imu_ms`. Consider having `checkFaults` read the sample timestamps so there is one source of truth.
- With a hot-plug IMU, `IMU_STALE` stays ON whenever the IMU is not fitted. Consider a "not fitted" state, or skipping the stale check while absent.
- `SD_*` recovery calls `sd.begin()` every 1 s (`RECOVER_PERIOD_MS`). With no card, each retry can block the loop briefly. A per-fault recovery period (e.g. 5 s for SD) would help.
- `SD_WRITE_FAIL` now means any SDLogger write, sync or open error. Please update its comment.

## Credits (patterns adapted, no code copied wholesale)
- SdFat TeensySdioLogger example: preAllocate + RingBuf, 512 B `writeOut` only when `!file.isBusy()`, `maxUsed`. https://github.com/greiman/SdFat/blob/master/examples/TeensySdioLogger/TeensySdioLogger.ino
- SdFat issue #513: array of preallocated files and one RingBuf, switched with `rb.begin(&file[i])`. https://github.com/greiman/sdfat/issues/513
- CAN-Insight: self-describing header, `max_file_size` rotation, `overwrite_logs` false by default, 1 s SD retry. https://github.com/puregame/CAN-Insight
- PJRC forum: size the buffer for worst-case card stalls (spec allows 750 ms). https://forum.pjrc.com/index.php?threads/maximum-average-throughput-to-sd-card-on-t4-1.71575/
- Research summary: `/workspace/fsae/Other-Teams-DAQ-Research.md` ("What we should steal" items 1, 2, 3, 5).
