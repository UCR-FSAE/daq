// SD logger bench test ONLY (env teensy41_sdbench). Not part of the real firmware.
// Fake sensors (bench_fakes.cpp) + Brian's fault timeline (fault_demo.cpp) drive the
// logger, the 1 Hz status record and fault records on a bare Teensy 4.1.
#include <Arduino.h>
#include "SensorSamples.h"
#include "SDLogger.h"
#include "faults.h"
#include "bench_fakes.h"

void faultDemoStep(uint32_t t);   // fault_demo.cpp

elapsedMillis current_time_ms;

void setup() {
    Serial.begin(115200);
    benchBegin();
    if (!sdLoggerBegin()) Serial.println("SD init failed (will retry every 1 s)");
    Serial.printf("SD startup %lu ms\n", (unsigned long)SDLogger::stats().startupMs);
}

void loop() {
    benchStep(current_time_ms);
    faultDemoStep(current_time_ms);
    logSensorsToSD();                 // also prints the 1 Hz STATUS line
    checkFaults(current_time_ms);
}
