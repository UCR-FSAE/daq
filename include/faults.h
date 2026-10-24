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

   // 1 entry per fault. Add new faults ABOVE FAULT_COUNT and add a row to
   // FAULT_INFO (faults.cpp) in the same order.
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

// What we do when a fault turns on. Combine with |, e.g. ACT_LOG | ACT_ALERT_PIT
// The DAQ can't shut the car down BMS/VCU/BSPD do that, so these are the options
enum FaultAction : uint8_t
{
    ACT_LOG          = 1 << 0, // print + (later) write to SD. Every fault gets this.
    ACT_INVALID      = 1 << 1, // that sensor's data can't be trusted while the fault is on
    ACT_RECOVER      = 1 << 2, // retry the sensor/peripheral every RECOVER_PERIOD_MS
    ACT_ALERT_PIT    = 1 << 3, // tell the pit over LoRa
    ACT_ALERT_DRIVER = 1 << 4, // show on the dash (dash link not designed yet)
};

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

uint8_t faultActions(Fault f);
bool isLogOnly(Fault f);   // true if the fault only gets logged, no action
bool dataValid(Fault f);   // false while an ACT_INVALID fault is on

// Other modules plug in what recover and alert actually do
// Nothing registered = that action is skipped (still logged).
using RecoverFn = void (*)();
using AlertFn   = void (*)(Fault f, bool active); // active = true when fault turns on
void setRecoverHandler(Fault f, RecoverFn fn);  // e.g. IMU module: re-run imu.begin()
void setPitAlertHandler(AlertFn fn);            // e.g. telemetry module: send over LoRa
void setDriverAlertHandler(AlertFn fn);         // e.g. dash module, later

void checkFaults(uint32_t now_ms); // call every loop(); it rate-limits itself