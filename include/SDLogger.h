#pragma once
#include <Arduino.h>
#include <SdFat.h>
#include <RingBuf.h>
#include "SensorSample.h"

// Non-blocking SD logger for Teensy 4.1 built-in SDIO slot.
// Pattern from SdFat's TeensySdioLogger example: preallocate file, buffer in a
// RingBuf, and only write whole 512 B sectors when the card is NOT busy.
//
// Record on disk: [0xA5][id u8][len u8][timestampMs u32][payload len bytes]
// File header: "FSAEDAQ\0", version u16, nTypes u8, then per type:
//   id u8, len u8, name[16], fmt[16] (Python struct format)

namespace SDLogger {

constexpr uint16_t FORMAT_VERSION = 1;
constexpr size_t   RING_BUF_SIZE  = 64 * 1024;           // ~0.8 s at 75 KB/s, >1 s below that
constexpr uint64_t PREALLOC_BYTES = 1ULL << 30;          // 1 GiB per file (~4 h at 75 KB/s)
constexpr size_t   SECTOR         = 512;
constexpr uint32_t SYNC_PERIOD_MS = 1000;                // flush FAT/dir entry once per second

struct TypeInfo { uint8_t id; uint8_t len; const char* name; const char* fmt; };

bool begin(const TypeInfo* types, uint8_t nTypes);       // opens LOGnnnnn.BIN (never overwrites)
bool writeRecord(uint8_t id, uint32_t tsMs, const void* payload, uint8_t len);

// Logs sample only if newSample is set; clears the flag. Returns true if logged.
template <typename T>
bool logIfNew(uint8_t id, SensorSample<T>& s) {
    if (!s.newSample) return false;
    noInterrupts(); T d = s.data; uint32_t ts = s.timestampMs; s.newSample = false; interrupts();
    return writeRecord(id, ts, &d, sizeof(T));
}

void service();        // call every loop(): writes one sector if card not busy
void stop();           // flush remaining, truncate, close
uint32_t droppedRecords();
size_t   maxBufferUsed();
bool     ok();

}
