#include "SensorHealth.h"

namespace SensorHealth {

struct Entry {
    const char*              name;
    const volatile uint32_t* lastMs;
    uint32_t                 timeoutMs;
    ProbeFn                  probe;
    PollFn                   poll;
    bool                     present;
    bool                     alive;
    uint32_t                 lastProbeMs;
    uint32_t                 presentSinceMs;
};

static Entry       entries[MAX_SENSORS];
static const char* nameList[MAX_SENSORS];
static uint8_t     n = 0;

uint8_t add(const char* name, const volatile uint32_t* lastMs, uint32_t timeoutMs, ProbeFn probe, PollFn poll) {
    if (n >= MAX_SENSORS) return 0xFF;
    entries[n] = {name, lastMs, timeoutMs, probe, poll, probe == nullptr, false, 0, 0};
    nameList[n] = name;
    return n++;
}

void service(uint32_t now) {
    for (uint8_t i = 0; i < n; i++) {
        Entry& e = entries[i];
        if (!e.present) {
            if (e.probe && (e.lastProbeMs == 0 || now - e.lastProbeMs >= PROBE_PERIOD_MS)) {
                e.lastProbeMs = now;
                if (e.probe()) { e.present = true; e.presentSinceMs = now; }
            }
            if (!e.present) { e.alive = false; continue; }
        }
        if (e.poll) e.poll();
        if (!e.lastMs) { e.alive = true; continue; }
        const uint32_t last = *e.lastMs;
        e.alive = last != 0 && (now - last) <= e.timeoutMs;
        // Hot-plug sensor gone quiet for a while: treat as unplugged, start probing again.
        if (e.probe && !e.alive) {
            const uint32_t quietSince = (last != 0 && last > e.presentSinceMs) ? last : e.presentSinceMs;
            if (now - quietSince > UNPLUG_MS) { e.present = false; e.lastProbeMs = now; }
        }
    }
}

uint8_t            count()  { return n; }
const char* const* names()  { return nameList; }

uint16_t presentMask() { uint16_t m = 0; for (uint8_t i = 0; i < n; i++) if (entries[i].present) m |= 1u << i; return m; }
uint16_t aliveMask()   { uint16_t m = 0; for (uint8_t i = 0; i < n; i++) if (entries[i].alive)   m |= 1u << i; return m; }

}
