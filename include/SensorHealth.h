#pragma once
#include <Arduino.h>

// Per-sensor health for the 1 Hz status record.
//  present = the device is plugged in / answered its probe (always true for sensors without a probe)
//  alive   = present AND its SensorSample timestamp is newer than its timeout
// Hot-plug sensors (IMU, LoRa) get a probe(): while absent it is retried once a second;
// once present, poll() runs every loop. If a hot-plug sensor stays stale for
// UNPLUG_MS it is marked absent again and probing restarts.
namespace SensorHealth {

using ProbeFn = bool (*)();   // true = device answered (e.g. I2C WHO_AM_I matched)
using PollFn  = void (*)();   // read the device and fill its SensorSample

constexpr uint8_t  MAX_SENSORS    = 16;     // status masks are uint16_t
constexpr uint32_t PROBE_PERIOD_MS = 1000;
constexpr uint32_t UNPLUG_MS      = 5000;

// lastMs: the sensor's timestamp (e.g. &imuSample.timestampMs, or Brian's &last_gps_ms).
// nullptr = no data stream (alive == present, e.g. LoRa radio).
uint8_t add(const char* name, const volatile uint32_t* lastMs, uint32_t timeoutMs,
            ProbeFn probe = nullptr, PollFn poll = nullptr);
void service(uint32_t now);     // call every loop()

uint8_t            count();
const char* const* names();     // bit order of the masks (also written into the SD header)
uint16_t           presentMask();
uint16_t           aliveMask();

}
