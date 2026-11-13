#include "SDLogger.h"
#include <string.h>
#include <stdlib.h>

#ifndef GIT_HASH
#define GIT_HASH "unknown"     // set by tools/pio_git_hash.py at build time
#endif

extern "C" uint32_t rtc_get(void);   // Teensy 4 core: RTC seconds (coin cell keeps it running)

namespace SDLogger {

static SdFs sd;
static FsFile files[FILE_POOL];                 // issue #513: array of preallocated files...
static RingBuf<FsFile, RING_BUF_SIZE> rb;       // ...and one RingBuf, re-pointed with rb.begin(&files[i])

static HeaderInfo hdrInfo{};
static uint8_t  cur = 0;                        // index of the file being written
static bool     spareReady = false;             // files[cur ^ 1] is open + preallocated
static uint16_t sparePart = 0;
static uint16_t curPart = 0;
static uint16_t nextPart = 0;
static uint32_t bootNo = 0;
static bool     bootChosen = false;

static bool     running = false;
static bool     cardOk = false;
static bool     full = false;
static bool     rotatePending = false;
static bool     syncPending = false;
static bool     inOverrun = false;
static bool     freeKnown = false;
static uint64_t freeEst = 0;
static uint32_t fileBytes = 0;                  // bytes queued for the current part (header included)
static uint32_t fileStartMs = 0;
static uint32_t lastArmTryMs = 0;
static elapsedMillis sinceSync;

static uint32_t droppedRecs = 0, droppedBytes = 0, overruns = 0, writeErrors = 0;
static uint32_t maxUsed = 0, maxServiceUs = 0, startupMs = 0;

constexpr uint32_t ARM_RETRY_MS   = 5000;   // retry preallocating the spare part this often if it failed
constexpr size_t   IDLE_BUF_BYTES = 4096;   // only do slow FS work when the RingBuf is nearly empty
constexpr uint16_t MAX_PART       = 999;

static void put(const void* p, size_t n) { rb.write((const uint8_t*)p, n); fileBytes += n; }

static void partName(char* out, size_t n, uint32_t boot, uint16_t part) {
    snprintf(out, n, "LOG%05lu_%03u.BIN", (unsigned long)boot, (unsigned)part);
}

// Parses "LOGbbbbb_ppp.BIN" (v2) or "LOGnnnnn.BIN" (v1). key = boot * 1000 + part.
static bool parseName(const char* nm, uint32_t& boot, uint16_t& part) {
    if (strncmp(nm, "LOG", 3) != 0) return false;
    char* end;
    unsigned long b = strtoul(nm + 3, &end, 10);
    if (end == nm + 3) return false;
    unsigned long p = 0;
    if (*end == '_') { char* e2; p = strtoul(end + 1, &e2, 10); end = e2; }
    if (strcasecmp(end, ".BIN") != 0) return false;
    boot = b; part = (uint16_t)p;
    return true;
}

// One pass over the root directory (much faster than probing sd.exists() for every number).
static void scanLogs(uint32_t* maxBoot, bool* any, char* oldest, size_t oldestLen, uint64_t* oldestSize) {
    FsFile root, f;
    if (any) *any = false;
    uint64_t bestKey = UINT64_MAX;
    if (oldest) oldest[0] = 0;
    if (!root.open("/")) return;
    char nm[32];
    while (f.openNext(&root, O_RDONLY)) {
        uint32_t b; uint16_t p;
        if (f.isFile() && f.getName(nm, sizeof(nm)) && parseName(nm, b, p)) {
            if (maxBoot && (!*any || b > *maxBoot)) *maxBoot = b;
            if (any) *any = true;
            const bool open = (b == bootNo && bootChosen && (p == curPart || (spareReady && p == sparePart)));
            const uint64_t key = (uint64_t)b * 1000 + p;
            if (oldest && !open && key < bestKey) {
                bestKey = key; strncpy(oldest, nm, oldestLen - 1); oldest[oldestLen - 1] = 0;
                if (oldestSize) *oldestSize = f.fileSize();
            }
        }
        f.close();
    }
    root.close();
}

static void computeFree() {
    freeEst = (uint64_t)sd.freeClusterCount() * sd.bytesPerCluster();   // slow on big FAT32 cards, done once
    freeKnown = true;
}

// SDLOG_DELETE_OLDEST is OFF by default: logs are never deleted or overwritten.
static void ensureRoom() {
#if SDLOG_DELETE_OLDEST
    if (!freeKnown) computeFree();
    for (int guard = 0; guard < 16 && freeEst < SDLOG_LOW_FREE_BYTES; guard++) {
        char nm[32]; uint64_t sz = 0;
        scanLogs(nullptr, nullptr, nm, sizeof(nm), &sz);
        if (!nm[0] || !sd.remove(nm)) break;
        freeEst += sz;
    }
#endif
}

static bool openPart(uint8_t slot, uint16_t part) {
    char nm[32];
    partName(nm, sizeof(nm), bootNo, part);
    if (!files[slot].open(nm, O_RDWR | O_CREAT | O_EXCL)) return false;     // O_EXCL: never overwrite
    if (!files[slot].preAllocate(SDLOG_FILE_BYTES)) {                        // contiguous, no cluster search mid-run
        files[slot].remove();                                                 // don't leave an empty stub behind
        return false;
    }
    if (freeKnown) freeEst = freeEst > SDLOG_FILE_BYTES ? freeEst - SDLOG_FILE_BYTES : 0;
    return true;
}

static void writeHeader() {
    const char magic[8] = {'F','S','A','E','D','A','Q','\0'};
    const HeaderInfo& h = hdrInfo;
    const uint16_t hdrLen = 8 + 2 + 2 + 4 + 2 + 4 + 4 + 16 + 4
                          + 1 + 34 * h.nTypes + 1 + 12 * h.nSensors + 1 + 16 * h.nFaults;
    const uint32_t startMs = millis();
    uint32_t rtc = rtc_get();
    if (rtc < 1577836800UL) rtc = 0;                     // before 2020 = RTC never set
    char git[16] = {0}; strncpy(git, GIT_HASH, 15);
    const uint32_t cap = SDLOG_FILE_BYTES;
    put(magic, 8); put(&FORMAT_VERSION, 2); put(&hdrLen, 2);
    put(&bootNo, 4); put(&curPart, 2); put(&startMs, 4); put(&rtc, 4); put(git, 16); put(&cap, 4);
    put(&h.nTypes, 1);
    for (uint8_t i = 0; i < h.nTypes; i++) {
        char nm[16] = {0}, fm[16] = {0};
        strncpy(nm, h.types[i].name, 15); strncpy(fm, h.types[i].fmt, 15);
        put(&h.types[i].id, 1); put(&h.types[i].len, 1); put(nm, 16); put(fm, 16);
    }
    put(&h.nSensors, 1);
    for (uint8_t i = 0; i < h.nSensors; i++) { char nm[12] = {0}; strncpy(nm, h.sensors[i], 11); put(nm, 12); }
    put(&h.nFaults, 1);
    for (uint8_t i = 0; i < h.nFaults; i++) { char nm[16] = {0}; strncpy(nm, h.faults[i], 15); put(nm, 16); }
}

static void fail() {
    writeErrors++;
    running = false; cardOk = false; rotatePending = syncPending = false;
    files[cur].close();
    if (spareReady) { files[cur ^ 1].remove(); spareReady = false; }
}

bool begin(const HeaderInfo& hdr) {
    if (running) return true;
    const uint32_t t0 = millis();
    hdrInfo = hdr;
    if (!sd.begin(SdioConfig(FIFO_SDIO))) { cardOk = false; return false; }
    cardOk = true;
    if (!bootChosen) {
        uint32_t maxBoot = 0; bool any = false;
        scanLogs(&maxBoot, &any, nullptr, 0, nullptr);
        bootNo = any ? maxBoot + 1 : 0;
        bootChosen = true; nextPart = 0;
    }
    ensureRoom();
    // Only ONE 64 MiB part is preallocated here (was 1 GiB): the spare is armed later from
    // service() when the buffer is nearly empty. This is what cuts the ~6 s startup.
    cur = 0; spareReady = false;
    if (nextPart > MAX_PART || !openPart(cur, nextPart)) { writeErrors++; full = true; return false; }
    curPart = nextPart++;
    rb.begin(&files[cur]);
    fileBytes = 0; fileStartMs = millis();
    writeHeader();
    running = true; full = false; rotatePending = syncPending = false; sinceSync = 0;
    startupMs = millis() - t0;
    return true;
}

bool writeRecord(uint8_t id, uint32_t tsMs, const void* payload, uint8_t len) {
    if (!running) return false;
    const size_t need = 7 + len;
    // Never block the loop. Also keep one sector free at the end of the part for padding.
    if (rb.bytesFree() < need || fileBytes + need > SDLOG_FILE_BYTES - SECTOR) {
        droppedRecs++; droppedBytes += need;
        if (!inOverrun) { overruns++; inOverrun = true; }
        if (fileBytes + need > SDLOG_FILE_BYTES - SECTOR && !spareReady) full = true;
        return false;
    }
    inOverrun = false;
    const uint8_t hdr[3] = {0xA5, id, len};
    put(hdr, 3); put(&tsMs, 4); put(payload, len);
    const size_t used = rb.bytesUsed(); if (used > maxUsed) maxUsed = used;
    if (!rotatePending && fileBytes > SDLOG_FILE_BYTES - 2 * RING_BUF_SIZE) rotatePending = true;
    return true;
}

// Fill the current sector with REC_PAD records so the 1 s sync puts EVERYTHING on the card.
static void padToSector() {
    size_t need = (SECTOR - (fileBytes % SECTOR)) % SECTOR;
    if (need == 0) return;
    if (need < 7) need += SECTOR;
    if (rb.bytesFree() < need || fileBytes + need > SDLOG_FILE_BYTES) return;
    static const uint8_t zeros[255] = {0};
    const uint32_t ts = millis();
    while (need > 0) {
        size_t chunk = need > 262 ? 262 : need;
        if (need - chunk > 0 && need - chunk < 7) chunk = need - 7;
        const uint8_t hdr[3] = {0xA5, REC_PAD, (uint8_t)(chunk - 7)};
        put(hdr, 3); put(&ts, 4); put(zeros, chunk - 7);
        need -= chunk;
    }
}

static void armSpare() {
    if (millis() - lastArmTryMs < ARM_RETRY_MS && lastArmTryMs != 0) return;
    lastArmTryMs = millis();
    if (nextPart > MAX_PART) { full = true; return; }
    ensureRoom();
    const uint8_t spare = cur ^ 1;
    if (openPart(spare, nextPart)) { sparePart = nextPart++; spareReady = true; lastArmTryMs = 0; }
    else full = true;   // card full (and deleting is off) or FS error; current part keeps going until its cap
}

static void rotate() {
    if (!spareReady) return;                              // keep filling the current part until the cap
    if (!rb.sync()) { fail(); return; }                   // < 1 sector left, card is idle
    const uint32_t used = fileBytes;
    if (!files[cur].truncate() || !files[cur].sync()) writeErrors++;
    files[cur].close();
    if (freeKnown) freeEst += SDLOG_FILE_BYTES - used;
    cur ^= 1; curPart = sparePart; spareReady = false;
    rb.begin(&files[cur]);                                // issue #513: same RingBuf, next preallocated file
    fileBytes = 0; fileStartMs = millis();
    writeHeader();
    rotatePending = false;
}

void service() {
    if (!running) return;
    const uint32_t t0 = micros();
    if ((SDLOG_FILE_MAX_MS) != 0 && !rotatePending && millis() - fileStartMs >= SDLOG_FILE_MAX_MS) rotatePending = true;
    if (!syncPending && sinceSync >= SYNC_PERIOD_MS) { padToSector(); syncPending = true; }

    if (!files[cur].isBusy()) {
        // TeensySdioLogger pattern: one 512 B sector, only when the card is idle.
        if (rb.bytesUsed() >= SECTOR) {
            if (rb.writeOut(SECTOR) != SECTOR) { fail(); return; }
        } else if (syncPending) {
            if (!files[cur].sync()) writeErrors++;
            syncPending = false; sinceSync = 0;
        } else if (rotatePending) {
            rotate();
        } else if (!spareReady && rb.bytesUsed() < IDLE_BUF_BYTES) {
            armSpare();
        } else if (!freeKnown && rb.bytesUsed() < IDLE_BUF_BYTES) {
            computeFree();
        }
    }
    const uint32_t dt = micros() - t0;
    if (dt > maxServiceUs) maxServiceUs = dt;
}

void stop() {
    if (!running) return;
    rb.sync(); files[cur].truncate(); files[cur].sync(); files[cur].close();
    if (spareReady) { files[cur ^ 1].remove(); spareReady = false; }
    running = false;
}

Stats stats() {
    Stats s{};
    s.droppedRecords = droppedRecs; s.droppedBytes = droppedBytes; s.overruns = overruns;
    s.writeErrors = writeErrors; s.maxBufUsed = maxUsed; s.bufUsed = running ? rb.bytesUsed() : 0;
    s.maxServiceUs = maxServiceUs;
    s.freeMB = freeKnown ? (uint32_t)(freeEst >> 20) : 0xFFFFFFFFUL;
    s.bootNo = bootNo; s.part = curPart; s.fileBytes = fileBytes; s.startupMs = startupMs;
    s.ok = running; s.full = full;
    return s;
}

bool ok()          { return running; }
bool cardPresent() { return cardOk; }

}
