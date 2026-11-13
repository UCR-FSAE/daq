#pragma once
#include <Arduino.h>
#include <elapsedMillis.h>
#include "SensorSample.h"

// Shared clock defined in main.cpp. Use it for every sensor timestamp.
extern elapsedMillis current_time_ms;

// One latest-sample slot per sensor. Each peripheral's .h/.cpp fills its slot
// (data + timestampMs = current_time_ms, then newSample = true).
// main.cpp's "write to sd card" step logs a slot only when its flag is set.
extern SensorSample<CarSpeedData>  carSpeedSample;
extern SensorSample<ImuData>       imuSample;
extern SensorSample<BrakeTempData> brakeTempSample;
extern SensorSample<ShockPotData>  shockPotSample;
extern SensorSample<SteeringData>  steeringSample;

// Starts the SD logger (new boot number, first rolling part), registers every sensor
// for health tracking and hooks the fault tracker into the log. Call once in setup().
// (implemented in DaqLog.cpp)
bool sdLoggerBegin();

// Polls hot-plug sensors, logs every sensor whose flag is set, writes the 1 Hz status
// record, then lets the SD card write if it's free. Non-blocking. Call once per loop().
void logSensorsToSD();
