// SD logger bench test ONLY (env teensy41_sdbench). Not part of the real firmware.
// Fake car speed (50 Hz) and IMU (100 Hz) so the logger can be tested on a bare Teensy 4.1.
#include <Arduino.h>
#include "SensorSamples.h"
#include "SDLogger.h"
#include "faults.h"

void faultDemoStep(uint32_t t);   // fault_demo.cpp

elapsedMillis current_time_ms;

static bool fakeWheelSpeed(float& speedMps) {
    static elapsedMillis t; if (t < 20) return false; t = 0;
    speedMps = 12.3f; return true;
}
static bool fakeImu(ImuData& d) {
    static elapsedMillis t; if (t < 10) return false; t = 0;
    d = {0, 0, 9.81f, 0, 0, 0}; return true;
}

void setup() {
    Serial.begin(115200);
    if (!sdLoggerBegin()) Serial.println("SD init failed");
    Serial.printf("SD startup %lu ms\n", (unsigned long)SDLogger::stats().startupMs);
}

void loop() {
    float v;
    if (fakeWheelSpeed(v)) {
        carSpeedSample.data.speedMps = v;
        carSpeedSample.timestampMs = current_time_ms;
        carSpeedSample.newSample = true;
    }
    if (fakeImu(imuSample.data)) { imuSample.timestampMs = current_time_ms; imuSample.newSample = true; last_imu_ms = current_time_ms; }
    faultDemoStep(current_time_ms);

    logSensorsToSD();

    static elapsedMillis report;
    if (report > 5000) {
        report = 0;
        SDLogger::Stats s = SDLogger::stats();
        Serial.printf("boot=%lu part=%u fileBytes=%lu dropped=%lu maxBuf=%lu maxSvc=%luus free=%luMB err=%lu ok=%d full=%d\n",
                      (unsigned long)s.bootNo, s.part, (unsigned long)s.fileBytes, (unsigned long)s.droppedRecords,
                      (unsigned long)s.maxBufUsed, (unsigned long)s.maxServiceUs, (unsigned long)s.freeMB,
                      (unsigned long)s.writeErrors, s.ok, s.full);
    }

    checkFaults(current_time_ms);
}
