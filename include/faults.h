#pragma once
#include <Arduino.h>

// ============================================================================
// DAQ Fault Tracking (32-bit bitmask, max 32 faults)
// - ISRs/drivers: write last-received timestamp only
// - checkFaults(): run in loop(); flag stale sensors after FAULT_CONFIRM_COUNT
// - setFault()/clearFault(): call from loop() only, never from ISRs
// - Logging: log state changes only
// - IDs: match the Fault Sheet
// ============================================================================

// 1 entry per fault. Add new faults before FAULT_COUNT and in the same order in FAULT_INFO (faults.cpp).
enum Fault : uint8_t {
    // stale data -> sensor stopped sending (set by checkFaults)
    FAULT_ADC_STALE = 0, // ADC-01  ADS8688 stopped returning data
    FAULT_TC_STALE,      // TC-01   2x coolant thermocouples (K-type, MCP960x on I2C)
    FAULT_IMU_STALE,     // IMU-01
    FAULT_GPS_STALE,     // // GPS-01  no valid RMC status 'A' for GPS_TIMEOUT_MS, TinyGPS age invalid
    FAULT_CAN_INV_STALE, // CAN-02  no 0x0AA from inverter
    FAULT_CAN_BMS_STALE, // CAN-03
    FAULT_CAN_VCU_STALE, // CAN-04

    // event faults -> set/cleared by the code that detects them
    FAULT_SD_INIT_FAIL,  // SD-01   SD.begin() returns false
    FAULT_SD_OPEN_FAIL,  // SD-02   log file failed to open
    FAULT_SD_WRITE_FAIL, // SD-03   bytes written != packet size
    FAULT_IMU_INIT_FAIL, // IMU-01  WHO_AM_I (0Fh) doesn't read 0x6B
    FAULT_TC_INIT_FAIL,  // TC-01   device ID (20h) doesn't read 0x40/0x41
    FAULT_CAN_BUS_OFF,   // CAN-01  FlexCAN error-passive or bus-off
    FAULT_SD_FULL,       // SD-04   no room for the next log part (logger never deletes by default)

    // Inverter faults (INV-xx) arrive via CAN 0x0AB and are logged by logInverterFaults().
    // BMS/VCU faults will be logged once their CAN IDs are defined.

    FAULT_COUNT
};
static_assert(FAULT_COUNT <= 32, "too many faults for a uint32_t");

// Actions when a fault occurs. Combine with | (e.g. ACT_LOG | ACT_ALERT_PIT).
// DAQ only logs/alerts. BMS/VCU/BSPD handle shutdowns.
enum FaultAction : uint8_t {
    ACT_LOG          = 1 << 0, // print + (later) write to SD. Every fault gets this.
    ACT_INVALID      = 1 << 1, // that sensor's data can't be trusted while the fault is on
    ACT_RECOVER      = 1 << 2, // retry the sensor/peripheral every RECOVER_PERIOD_MS
    ACT_ALERT_PIT    = 1 << 3, // tell the pit over LoRa
    ACT_ALERT_DRIVER = 1 << 4, // show on the dash (dash link not designed yet)
};

// Last sensor update times in ms (current_time_ms). Set by ISRs/drivers, read by checkFaults().
// 32-bit access is atomic on Teensy 4.1. volatile is enough.
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
uint32_t getFaults(); // all faults as 1 number, for logging/CAN
const char *faultName(Fault f);

uint8_t faultActions(Fault f);
bool isLogOnly(Fault f);   // true if the fault only gets logged, no action
bool dataValid(Fault f);   // false while an ACT_INVALID fault is on

// Other modules define recovery and alert actions.
// If not registered, skip the action but still log the fault.
using RecoverFn = void (*)();
using AlertFn   = void (*)(Fault f, bool active); // active = true when fault turns on
void setRecoverHandler(Fault f, RecoverFn fn);  // e.g. IMU module: re-run imu.begin()
void setPitAlertHandler(AlertFn fn);            // e.g. telemetry module: send over LoRa
void setDriverAlertHandler(AlertFn fn);         // e.g. dash module, later

// ---------------------------------------------------------------------------
// SD logger handles saving fault records.
// If not registered, records only print to Serial.
// ---------------------------------------------------------------------------
enum FaultRecord : uint8_t
{
    FREC_DAQ_FAULTS = 0, // our uint32_t fault bits, 4 bytes
    FREC_INV_FAULTS,     // raw inverter fault msg 0x0AB, 8 bytes
};
using LogFn = void (*)(FaultRecord type, uint32_t time_ms, const void *data, uint8_t len);
void setLogHandler(LogFn fn); // e.g. main: forward to SDLogger::writeRecord

// Inverter faults (INV-01..54) arrive in CAN message 0x0AB. Pass 4 words from CanParser to log all 8 bytes.
// Log at startup and whenever the data changes.
void logInverterFaults(uint16_t postFaultLo, uint16_t postFaultHi, uint16_t runFaultLo, uint16_t runFaultHi, uint32_t now_ms);

void checkFaults(uint32_t now_ms); // call every loop(); it rate-limits itself