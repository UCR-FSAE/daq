#pragma once
#include <Arduino.h>

// Every interrupt/periodically-sampled sensor exposes one of these.
// The driver fills data + timestampMs and sets newSample=true.
// The SD logger writes data+timestamp only when newSample is true, then clears it.
template <typename T>
struct SensorSample {
    uint32_t timestampMs = 0;   // from the shared elapsedMillis current_time_ms (main.cpp)
    T        data{};            // human units (m/s, degC, g, ...)
    volatile bool newSample = false;
};

// Record IDs (keep in sync with tools/decode_sdlog.py and the file header table)
enum RecordId : uint8_t {
    REC_PAD        = 0,   // filler before each 1 s sync (SDLogger), decoder skips it
    REC_CAR_SPEED  = 1,   // post-processed car speed only (not raw pulses)
    REC_IMU        = 2,
    REC_BRAKE_TEMP = 3,
    REC_SHOCK_POT  = 4,
    REC_STEERING   = 5,
    REC_FAULT_DAQ  = 20,  // faults.h FREC_DAQ_FAULTS: uint32 fault bitmask, on every change
    REC_FAULT_INV  = 21,  // faults.h FREC_INV_FAULTS: raw inverter CAN 0x0AB (4x uint16), on change
    REC_PIT_ALERT  = 22,  // a fault asked for a pit (LoRa) alert: fault id, on/off, sent over LoRa?
    REC_STATUS     = 250, // 1 Hz logger + sensor health status
    REC_EVENT      = 255  // text/marker
};

// Payloads (packed, little-endian, human units)
struct __attribute__((packed)) CarSpeedData { float speedMps; };                       // "<f"
struct __attribute__((packed)) ImuData      { float ax, ay, az, gx, gy, gz; };         // "<6f"
struct __attribute__((packed)) BrakeTempData{ float tempC[4]; };                       // "<4f"
struct __attribute__((packed)) ShockPotData { float travelMm[4]; };                    // "<4f"
struct __attribute__((packed)) SteeringData { float angleDeg; };                       // "<f"
struct __attribute__((packed)) InvFaultData { uint16_t postLo, postHi, runLo, runHi; };  // "<4H"
struct __attribute__((packed)) PitAlertData { uint8_t fault, active, sent; };            // "<3B"

// 1 Hz status record. Bit i of the masks = sensor i in the header's sensor list.
struct __attribute__((packed)) StatusData {                                            // "<3H8IHII"
    uint16_t seq;            // increments every record (gaps = lost seconds)
    uint16_t presentMask;    // sensor plugged in / answered its probe
    uint16_t aliveMask;      // sensor sent fresh data within its timeout (present but not alive = STALE)
    uint32_t sdFreeMB;       // estimated free space, 0xFFFFFFFF = not measured yet
    uint32_t writeErrors;    // SD write/sync/open failures since boot
    uint32_t droppedBytes;   // bytes refused because the RAM buffer was full
    uint32_t droppedRecords;
    uint32_t bufMaxUsed;     // RingBuf high-water mark (bytes, of 65536)
    uint32_t bufUsed;        // bytes waiting in RAM right now
    uint32_t overruns;       // separate buffer-full episodes
    uint32_t faults;         // Brian's DAQ fault bitmask (getFaults())
    uint16_t filePart;       // current LOGbbbbb_ppp part
    uint32_t fileBytes;      // bytes in the current part
    uint32_t maxServiceUs;   // longest SDLogger::service() call (card stalls, preallocation)
};
