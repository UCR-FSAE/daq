#pragma once
#include <Arduino.h>

// ============================================================================
// DAQ fault tracking
//   - Every fault is 1 bit in a uint32_t (max 32 faults).
//   - Sensor ISRs/drivers ONLY write their "last data received" timestamp.
//   - checkFaults() runs from loop() every FAULT_CHECK_PERIOD_MS and decides
//     which "stale data" faults are on. A sensor must be stale for
//     FAULT_CONFIRM_COUNT checks in a row before its fault turns on.
//   - Event faults (SD fail, IMU init fail, CAN bus-off, thermocouple open)
//     are set/cleared by the code that detects them, via setFault().
//     Call setFault()/clearFault() from loop() code only, never from an ISR.
//   - Only CHANGES are logged, so the log stays small.
// ============================================================================

// One entry per fault. Add new faults ABOVE FAULT_COUNT and add a name in
// FAULT_NAMES (faults.cpp) in the same order.
enum Fault : uint8_t
{
    // stale data: sensor stopped sending (set by checkFaults)
    FAULT_WHEEL_SPEED_STALE = 0, // WSS-01  hook to Will's 20ms update, not pulses
    FAULT_ADC_STALE,             // ADC-01  shock pots + steering (ADS8688)
    FAULT_TC_STALE,              // TC-01   6x MCP960x on I2C (confirm brake IR vs TC)
    FAULT_IMU_STALE,             // IMU-01
    FAULT_GPS_STALE,             // GPS-01  TinyGPS fix age invalid
    FAULT_CAN_INV_STALE,         // CAN-02  no 0x0AA
    FAULT_CAN_BMS_STALE,         // CAN-03
    FAULT_CAN_VCU_STALE,         // CAN-04

    // event faults: set/cleared by the code that detects them
    FAULT_SD_INIT_FAIL,     // SD-01
    FAULT_SD_OPEN_FAIL,     // SD-02
    FAULT_SD_WRITE_FAIL,    // SD-03
    FAULT_IMU_INIT_FAIL,    // IMU-01
    FAULT_CAN_BUS_OFF,      // CAN-01
    FAULT_COOLANT_TC1_OPEN, // needs MCP9601 (-01) chips
    FAULT_COOLANT_TC2_OPEN,

    FAULT_COUNT
};
static_assert(FAULT_COUNT <= 32, "too many faults for a uint32_t");

// "Last time we got data" timestamps, in ms (use current_time_ms).
// Written by sensor ISRs/drivers, read by checkFaults().
// 32-bit reads/writes are atomic on the Teensy 4.1, so volatile is enough.
extern volatile uint32_t last_wheel_speed_ms;
extern volatile uint32_t last_adc_ms;
extern volatile uint32_t last_tc_ms;
extern volatile uint32_t last_imu_ms;
extern volatile uint32_t last_gps_ms;
extern volatile uint32_t last_can_inv_ms;
extern volatile uint32_t last_can_bms_ms;
extern volatile uint32_t last_can_vcu_ms;

void setFault(Fault f);
void clearFault(Fault f);
bool hasFault(Fault f);
uint32_t getFaults(); // all faults as one number, for logging/CAN
const char *faultName(Fault f);

void checkFaults(uint32_t now_ms); // call every loop(); it rate-limits itself