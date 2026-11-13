#include "bench_fakes.h"
#include "SensorSamples.h"
#include "faults.h"

#ifndef BENCH_AUTO_SCRIPT
#define BENCH_AUTO_SCRIPT 1
#endif

static bool imuOn = true, wheelOn = true, gpsOn = false, loraOn = false;
static bool simOut = false, autoScript = BENCH_AUTO_SCRIPT;

// Strong versions of the weak hot-plug hooks in DaqLog.cpp.
bool imuProbe() { return imuOn; }
void imuPoll() {
    static uint32_t last = 0;
    const uint32_t t = current_time_ms;
    if (!imuOn || t - last < 10) return;                  // 100 Hz
    last = t;
    const float s = t * 0.001f;
    imuSample.data = {0.3f * sinf(s), 0.8f * sinf(0.5f * s), 9.81f, 0.0f, 0.0f, 0.2f * sinf(0.5f * s)};
    imuSample.timestampMs = t;
    imuSample.newSample = true;
}
bool loraProbe() { return loraOn; }
void loraSendAlert(const char* msg) { Serial.printf("  -> LoRa (fake) sent: %s\n", msg); }

static void help() {
    Serial.println("bench keys: i=IMU w=wheel g=GPS l=LoRa v=SIM output a=auto script off h=help");
}

void benchBegin() {
    Serial.println("SD logger bench: fake IMU + wheel speed, GPS off, LoRa unplugged");
    help();
}

static void script(uint32_t t) {
    static uint8_t step = 0;
    if (!autoScript) return;
    switch (step) {
        case 0: if (t >= 10000) { imuOn = false;  Serial.println("SCRIPT: IMU unplugged"); step++; } break;
        case 1: if (t >= 20000) { imuOn = true;   Serial.println("SCRIPT: IMU plugged in"); step++; } break;
        case 2: if (t >= 25000) { loraOn = true;  Serial.println("SCRIPT: LoRa plugged in"); step++; } break;
        case 3: if (t >= 30000) { wheelOn = false; Serial.println("SCRIPT: wheel speed off"); step++; } break;
        case 4: if (t >= 35000) { wheelOn = true; Serial.println("SCRIPT: wheel speed back"); step++; } break;
        default: break;
    }
}

static void keys() {
    while (Serial.available()) {
        switch (Serial.read()) {
            case 'i': imuOn = !imuOn;     Serial.printf("IMU %s\n", imuOn ? "on" : "off"); break;
            case 'w': wheelOn = !wheelOn; Serial.printf("wheel %s\n", wheelOn ? "on" : "off"); break;
            case 'g': gpsOn = !gpsOn;     Serial.printf("GPS %s\n", gpsOn ? "on" : "off"); break;
            case 'l': loraOn = !loraOn;   Serial.printf("LoRa %s\n", loraOn ? "on" : "off"); break;
            case 'v': simOut = !simOut; break;
            case 'a': autoScript = false; Serial.println("auto script off"); break;
            case 'h': help(); break;
            default: break;
        }
    }
}

void benchStep(uint32_t t) {
    keys();
    script(t);

    static uint32_t lastWheel = 0;                         // 50 Hz post-processed car speed
    if (wheelOn && t - lastWheel >= 20) {
        lastWheel = t;
        carSpeedSample.data.speedMps = 12.0f + 3.0f * sinf(t * 0.0005f);
        carSpeedSample.timestampMs = t;
        carSpeedSample.newSample = true;
    }
    if (gpsOn) last_gps_ms = t;                            // fake GPS fix (1 Hz is enough for the stale check)

    // SIM output: what the sensors would send, readable without GPS or a laptop decoder.
    static uint32_t lastSim = 0;
    if (simOut && t - lastSim >= 100) {
        lastSim = t;
        const ImuData& d = imuSample.data;
        Serial.printf("SIM,%lu,speed=%.2f,ax=%.2f,ay=%.2f,az=%.2f,gz=%.2f,imu=%d,wheel=%d,gps=%d,lora=%d\n",
                      (unsigned long)t, carSpeedSample.data.speedMps, d.ax, d.ay, d.az, d.gz, imuOn, wheelOn, gpsOn, loraOn);
    }
}
