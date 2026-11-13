#pragma once
#include "SDLogger.h"

// Every record type the SD logger knows about. Written into each log file's
// header so tools/decode_sdlog.py can decode it. Add a row when you add a sensor.
static const SDLogger::TypeInfo kLogTypes[] = {
    {REC_CAR_SPEED,  sizeof(CarSpeedData),  "car_speed",  "<f"},
    {REC_IMU,        sizeof(ImuData),       "imu",        "<6f"},
    {REC_BRAKE_TEMP, sizeof(BrakeTempData), "brake_temp", "<4f"},
    {REC_SHOCK_POT,  sizeof(ShockPotData),  "shock_pot",  "<4f"},
    {REC_STEERING,   sizeof(SteeringData),  "steering",   "<f"},
    {REC_FAULT_DAQ,  sizeof(uint32_t),      "fault_daq",  "<I"},
    {REC_FAULT_INV,  sizeof(InvFaultData),  "fault_inv",  "<4H"},
    {REC_PIT_ALERT,  sizeof(PitAlertData),  "pit_alert",  "<3B"},
    {REC_STATUS,     sizeof(StatusData),    "status",     "<3H8IHII"},
};
static const uint8_t kNumLogTypes = sizeof(kLogTypes) / sizeof(kLogTypes[0]);

