#include <Arduino.h>
#include "SensorSample.h"
#include "SDLogger.h"
// Example integration. WheelSpeed / IMU calls are stubbed so this builds on its own;
// on the real branch replace the stubs with WheelSpeed::update()/getData() etc.

elapsedMillis daqClock;                    // one shared millisecond clock for all timestamps

SensorSample<CarSpeedData> carSpeed;
SensorSample<ImuData>      imu;

static const SDLogger::TypeInfo kTypes[] = {
    {REC_CAR_SPEED, sizeof(CarSpeedData), "car_speed", "<f"},
    {REC_IMU,       sizeof(ImuData),      "imu",       "<6f"},
};

// --- stub for WheelSpeed::update(): returns true on a fresh 50 Hz sample ---
static bool wheelSpeedUpdate(float& speedMps) {
    static elapsedMillis t; if (t < 20) return false; t = 0;
    speedMps = 12.3f; return true;
}
// --- stub IMU at 100 Hz ---
static bool imuUpdate(ImuData& d) {
    static elapsedMillis t; if (t < 10) return false; t = 0;
    d = {0, 0, 9.81f, 0, 0, 0}; return true;
}

void setup() {
    Serial.begin(115200);
    if (!SDLogger::begin(kTypes, sizeof(kTypes) / sizeof(kTypes[0])))
        Serial.println("SD init failed");
}

void loop() {
    float v;
    if (wheelSpeedUpdate(v)) {                      // WheelSpeed-style: true = new data
        carSpeed.data.speedMps = v;                 // post-processed car speed only
        carSpeed.timestampMs = daqClock;
        carSpeed.newSample = true;
    }
    if (imuUpdate(imu.data)) { imu.timestampMs = daqClock; imu.newSample = true; }

    SDLogger::logIfNew(REC_CAR_SPEED, carSpeed);    // writes only if flag set, then clears
    SDLogger::logIfNew(REC_IMU, imu);
    SDLogger::service();                            // non-blocking sector write

    // TODO: stop() on LV-off detect so the last buffer is flushed.
}
