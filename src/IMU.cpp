#include "IMU.h"

#include <ICM_20948.h>
#include <Wire.h>

namespace IMU {

namespace {

// Teensy 4.1 IMU I2C pinout: SDA 25, SCL 24 (Wire2).
constexpr uint8_t SDA_PIN = 25;
constexpr uint8_t SCL_PIN = 24;
constexpr bool I2C_AD0_HIGH = true; // Address 0x69; false selects 0x68.

constexpr uint16_t SAMPLE_RATE_HZ = 100;
constexpr uint32_t POLL_INTERVAL_US = 5000;
constexpr float MG_TO_M_S2 = 0.00980665f;
constexpr float DEGREES_TO_RADIANS = 0.017453292519943295f;
constexpr uint8_t EXPECTED_WHO_AM_I = 0xEA;
constexpr uint8_t ACC_GYRO_MASK =
    ICM_20948_Internal_Acc | ICM_20948_Internal_Gyr;

// Identity until the board's physical mounting orientation is verified.
constexpr float SENSOR_TO_VEHICLE[3][3] = {
    {1.0f, 0.0f, 0.0f},
    {0.0f, 1.0f, 0.0f},
    {0.0f, 0.0f, 1.0f},
};

ICM_20948_I2C sensor;
Measurement data = {};
Diagnostics diagnostics = {};
bool initialized = false;
uint32_t lastPollUs = 0;

bool configureSensor() {
    if (sensor.setSampleMode(ACC_GYRO_MASK,
                             ICM_20948_Sample_Mode_Continuous) !=
        ICM_20948_Stat_Ok) {
        return false;
    }

    ICM_20948_fss_t fullScale;
    fullScale.a = gpm2;
    fullScale.g = dps250;
    if (sensor.setFullScale(ACC_GYRO_MASK, fullScale) != ICM_20948_Stat_Ok) {
        return false;
    }

    ICM_20948_dlpcfg_t filterConfig;
    filterConfig.a = acc_d23bw9_n34bw4;
    filterConfig.g = gyr_d23bw9_n35bw9;

    if (sensor.setDLPFcfg(ACC_GYRO_MASK, filterConfig) != ICM_20948_Stat_Ok ||
        sensor.enableDLPF(ACC_GYRO_MASK, true) != ICM_20948_Stat_Ok) {
        return false;
    }

    // With DLPF enabled, accel and gyro use nominal 1125 Hz and 1100 Hz base
    // rates. A divider of 10 produces approximately 102 Hz accel and 100 Hz
    // gyro. The library uses separate divider fields for the two sensors.
    constexpr uint16_t accelDivider = (1125 / SAMPLE_RATE_HZ) - 1;
    constexpr uint8_t gyroDivider = (1100 / SAMPLE_RATE_HZ) - 1;
    ICM_20948_smplrt_t sampleRate;
    sampleRate.a = accelDivider;
    sampleRate.g = gyroDivider;
    if (sensor.setSampleRate(ACC_GYRO_MASK, sampleRate) != ICM_20948_Stat_Ok) {
        return false;
    }

    return true;
}

}

bool begin() {
    Wire2.setSDA(SDA_PIN);
    Wire2.setSCL(SCL_PIN);
    Wire2.begin();
    Wire2.setClock(400000);

    if (sensor.begin(Wire2, I2C_AD0_HIGH) != ICM_20948_Stat_Ok ||
        sensor.getWhoAmI() != EXPECTED_WHO_AM_I || !configureSensor()) {
        initialized = false;
        diagnostics.initialized = false;
        diagnostics.sample_valid = false;
        ++diagnostics.communication_errors;
        return false;
    }

    initialized = true;
    diagnostics.initialized = true;
    diagnostics.sample_valid = false;
    lastPollUs = micros() - POLL_INTERVAL_US;
    return true;
}

bool update() {
    if (!initialized) {
        diagnostics.sample_valid = false;
        return false;
    }

    const uint32_t nowUs = micros();
    if (static_cast<uint32_t>(nowUs - lastPollUs) < POLL_INTERVAL_US) {
        return false;
    }
    lastPollUs = nowUs;

    if (!sensor.dataReady()) {
        if (sensor.status != ICM_20948_Stat_NoData) {
            diagnostics.sample_valid = false;
            ++diagnostics.communication_errors;
        }
        return false;
    }

    sensor.getAGMT();
    if (sensor.status != ICM_20948_Stat_Ok) {
        diagnostics.sample_valid = false;
        ++diagnostics.communication_errors;
        return false;
    }

    const float rawAccel[3] = {sensor.accX(), sensor.accY(), sensor.accZ()};
    const float rawGyro[3] = {sensor.gyrX(), sensor.gyrY(), sensor.gyrZ()};
    float vehicleAccel[3] = {};
    float vehicleGyro[3] = {};
    for (uint8_t row = 0; row < 3; ++row) {
        for (uint8_t column = 0; column < 3; ++column) {
            vehicleAccel[row] += SENSOR_TO_VEHICLE[row][column] * rawAccel[column];
            vehicleGyro[row] += SENSOR_TO_VEHICLE[row][column] * rawGyro[column];
        }
    }

    data.timestamp_us = micros();
    data.accel_x = vehicleAccel[0] * MG_TO_M_S2;
    data.accel_y = vehicleAccel[1] * MG_TO_M_S2;
    data.accel_z = vehicleAccel[2] * MG_TO_M_S2;
    data.gyro_x = vehicleGyro[0] * DEGREES_TO_RADIANS;
    data.gyro_y = vehicleGyro[1] * DEGREES_TO_RADIANS;
    data.gyro_z = vehicleGyro[2] * DEGREES_TO_RADIANS;
    data.raw_accel_x_mg = rawAccel[0];
    data.raw_accel_y_mg = rawAccel[1];
    data.raw_accel_z_mg = rawAccel[2];
    data.raw_gyro_x_dps = rawGyro[0];
    data.raw_gyro_y_dps = rawGyro[1];
    data.raw_gyro_z_dps = rawGyro[2];
    diagnostics.sample_valid = true;

    return true;
}

const Measurement& getData() {
    return data;
}

const Diagnostics& getDiagnostics() {
    return diagnostics;
}

}
