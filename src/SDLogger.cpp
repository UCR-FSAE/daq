#include "SDLogger.h"

namespace SDLogger {

static SdFs sd;
static FsFile file;
static RingBuf<FsFile, RING_BUF_SIZE> rb;
static bool     running = false;
static uint32_t dropped = 0;
static size_t   maxUsed = 0;
static elapsedMillis sinceSync;

static bool openNext(char* name) {
    for (uint32_t i = 0; i < 100000; i++) {
        snprintf(name, 16, "LOG%05lu.BIN", (unsigned long)i);
        if (!sd.exists(name)) return file.open(name, O_RDWR | O_CREAT | O_EXCL);
    }
    return false;
}

static void put(const void* p, size_t n) { rb.write((const uint8_t*)p, n); }

bool begin(const TypeInfo* types, uint8_t nTypes) {
    if (!sd.begin(SdioConfig(FIFO_SDIO))) return false;
    char name[16];
    if (!openNext(name)) return false;
    if (!file.preAllocate(PREALLOC_BYTES)) { file.close(); return false; }
    rb.begin(&file);
    const char magic[8] = {'F','S','A','E','D','A','Q','\0'};
    put(magic, 8); put(&FORMAT_VERSION, 2); put(&nTypes, 1);
    for (uint8_t i = 0; i < nTypes; i++) {
        char nm[16] = {0}, fm[16] = {0};
        strncpy(nm, types[i].name, 15); strncpy(fm, types[i].fmt, 15);
        put(&types[i].id, 1); put(&types[i].len, 1); put(nm, 16); put(fm, 16);
    }
    running = true; sinceSync = 0;
    return true;
}

bool writeRecord(uint8_t id, uint32_t tsMs, const void* payload, uint8_t len) {
    if (!running) return false;
    const size_t need = 7 + len;
    if (rb.bytesFree() < need) { dropped++; return false; }   // never block the loop
    const uint8_t hdr[3] = {0xA5, id, len};
    put(hdr, 3); put(&tsMs, 4); put(payload, len);
    size_t used = rb.bytesUsed(); if (used > maxUsed) maxUsed = used;
    return true;
}

void service() {
    if (!running) return;
    // Only write a full sector when the card is idle -> loop never waits on flash.
    if (rb.bytesUsed() >= SECTOR && !file.isBusy()) {
        if (rb.writeOut(SECTOR) != SECTOR) { running = false; return; }
    }
    if (sinceSync >= SYNC_PERIOD_MS && !file.isBusy() && rb.bytesUsed() < SECTOR) {
        // file size is the preallocated size; data length is recovered by the
        // decoder (stops at first non-0xA5 byte). Periodic sync keeps FS consistent.
        file.sync(); sinceSync = 0;
    }
}

void stop() {
    if (!running) return;
    rb.sync(); file.truncate(); file.sync(); file.close(); running = false;
}

uint32_t droppedRecords() { return dropped; }
size_t   maxBufferUsed()  { return maxUsed; }
bool     ok()             { return running; }

}
