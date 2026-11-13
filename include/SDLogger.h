#pragma once
#include <Arduino.h>
#include <SdFat.h>
#include <RingBuf.h>
#include "SensorSample.h"

// Non-blocking SD logger for the Teensy 4.1 built-in SDIO slot.
//
// Adapted from (credits + URLs in SD-LOGGER-UPDATE.md):
//  - SdFat TeensySdioLogger example: preAllocate + RingBuf, write one 512 B
//    sector only when !file.isBusy(), track the RingBuf high-water mark.
//  - SdFat issue #513: keep an array of preallocated files and ONE RingBuf,
//    switch files with rb.begin(&file[i]) instead of growing one huge file.
//  - CAN-Insight: rotate at max_file_size, self-describing header,
//    overwrite_logs defaults to false (we never delete unless compiled in).
//
// Files: LOGbbbbb_ppp.BIN, bbbbb = boot number, ppp = part within that boot.
// Each part is a complete log on its own (it starts with its own header).
//
// Record on disk: [0xA5][id u8][len u8][timestampMs u32][payload len bytes]
// id 0 (REC_PAD) is filler written before each 1 s sync; decoders skip it.
//
// File header v2 (little endian):
//   "FSAEDAQ\0", version u16, header_len u16, boot u32, part u16,
//   start_ms u32, rtc_unix u32 (0 = RTC not set), git[16], file_cap u32,
//   nTypes u8,   nTypes   x { id u8, len u8, name[16], fmt[16] (Python struct) }
//   nSensors u8, nSensors x name[12]   (bit order of the status alive mask)
//   nFaults u8,  nFaults  x name[16]   (bit order of the DAQ fault bitmask)

// ---- compile-time options (override with build_flags = -D...) ----
#ifndef SDLOG_FILE_BYTES
#define SDLOG_FILE_BYTES (64UL * 1024 * 1024)   // preallocated size of each part (64 MiB ~ 14 min at 75 KB/s)
#endif
#ifndef SDLOG_FILE_MAX_MS
#define SDLOG_FILE_MAX_MS (15UL * 60 * 1000)    // also start a new part after this long (0 = size only)
#endif
#ifndef SDLOG_DELETE_OLDEST
#define SDLOG_DELETE_OLDEST 0                   // 0 = NEVER delete/overwrite (default). 1 = delete oldest part when free space is low
#endif
#ifndef SDLOG_LOW_FREE_BYTES
#define SDLOG_LOW_FREE_BYTES (3ULL * SDLOG_FILE_BYTES)  // "low" threshold for SDLOG_DELETE_OLDEST
#endif

namespace SDLogger {

constexpr uint16_t FORMAT_VERSION = 2;
constexpr size_t   RING_BUF_SIZE  = 64 * 1024;   // ~0.8 s at 75 KB/s; covers most card stalls (spec allows 750 ms)
constexpr size_t   SECTOR         = 512;
constexpr uint32_t SYNC_PERIOD_MS = 1000;        // pad + flush + sync every second -> power cut loses <= ~1 s
constexpr uint8_t  FILE_POOL      = 2;           // [current, spare] (issue #513 file array)

static_assert(SDLOG_FILE_BYTES >= 4 * RING_BUF_SIZE, "parts must be much bigger than the RAM buffer");

struct TypeInfo { uint8_t id; uint8_t len; const char* name; const char* fmt; };

struct HeaderInfo {
    const TypeInfo*    types;   uint8_t nTypes;
    const char* const* sensors; uint8_t nSensors;   // may be null/0
    const char* const* faults;  uint8_t nFaults;    // may be null/0
};

struct Stats {
    uint32_t droppedRecords;  // records refused because the RingBuf was full
    uint32_t droppedBytes;
    uint32_t overruns;        // number of separate "buffer full" episodes
    uint32_t writeErrors;     // failed writeOut / sync / open / preallocate
    uint32_t maxBufUsed;      // RingBuf high-water mark (bytes) since boot
    uint32_t bufUsed;         // bytes waiting in RAM right now
    uint32_t maxServiceUs;    // longest sector write / sync in service() (card stall indicator)
    uint32_t lastPreallocMs;  // how long preallocating the last spare part took (blocks loop once per part)
    uint32_t freeMB;          // estimated free space on the card
    uint32_t bootNo;
    uint16_t part;
    uint32_t fileBytes;       // bytes written into the current part
    uint32_t startupMs;       // how long begin() took
    bool     ok;              // logging right now
    bool     full;            // no room for a new part (and deleting is off)
};

// Opens LOGbbbbb_000.BIN with a fresh boot number. Never overwrites.
// Safe to call again after a failure (keeps the boot number, opens the next part).
bool begin(const HeaderInfo& hdr);
bool writeRecord(uint8_t id, uint32_t tsMs, const void* payload, uint8_t len);

// Logs sample only if newSample is set; clears the flag. Returns true if logged.
template <typename T>
bool logIfNew(uint8_t id, SensorSample<T>& s) {
    if (!s.newSample) return false;
    noInterrupts(); T d = s.data; uint32_t ts = s.timestampMs; s.newSample = false; interrupts();
    return writeRecord(id, ts, &d, sizeof(T));
}

void  service();       // call every loop(): <=1 sector write, 1 s sync, rotation, spare preallocation
void  stop();          // flush, truncate, close
Stats stats();
bool  ok();
bool  cardPresent();   // sd.begin() succeeded at least once since the last failure

}
