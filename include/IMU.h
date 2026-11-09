#pragma once

#include <Arduino.h>

namespace IMU {

struct Measurement {
    uint32_t timestamp_us;
    float accel_x;
    float accel_y;
    float accel_z;
    float gyro_x;
    float gyro_y;
    float gyro_z;
    float raw_accel_x_mg;
    float raw_accel_y_mg;
    float raw_accel_z_mg;
    float raw_gyro_x_dps;
    float raw_gyro_y_dps;
    float raw_gyro_z_dps;
};

struct Diagnostics {
    bool initialized;
    bool sample_valid;
    uint32_t communication_errors;
};

// Initialize the ICM-20948 on Wire2 using the configured pinout.
bool begin();

// Poll for a fresh sample. Returns true only when data has been updated.
bool update();

// Access the most recent measurement.
const Measurement& getData();

// Access communication and latest-sample status.
const Diagnostics& getDiagnostics();

}
