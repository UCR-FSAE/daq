#include "faults.h"

// ---------------------------------------------------------------------------
// All PLACEHOLDERS until we know each sensor's real sample rate.
// Rule of thumb: timeout = ~3-5x the sensor's normal time between samples.
// ---------------------------------------------------------------------------
static constexpr uint32_t FAULT_CHECK_PERIOD_MS = 10; // run checks at 100 Hz
static constexpr uint8_t FAULT_CONFIRM_COUNT = 3;     // 3 bad checks in a row = real fault
static constexpr uint32_t STARTUP_GRACE_MS = 2000;    // let sensors boot before checking

static constexpr uint32_t WHEEL_SPEED_TIMEOUT_MS = 50; // Will's module updates every 20 ms
static constexpr uint32_t ADC_TIMEOUT_MS = 50;
static constexpr uint32_t TC_TIMEOUT_MS = 500;         // temps are slow
static constexpr uint32_t IMU_TIMEOUT_MS = 50;
static constexpr uint32_t GPS_TIMEOUT_MS = 1000;
static constexpr uint32_t CAN_INV_TIMEOUT_MS = 100;
static constexpr uint32_t CAN_BMS_TIMEOUT_MS = 100;
static constexpr uint32_t CAN_VCU_TIMEOUT_MS = 100;

// Same order as the Fault enum.
static const char *const FAULT_NAMES[FAULT_COUNT] = {
    "WHEEL_SPEED_STALE",
    "ADC_STALE",
    "TC_STALE",
    "IMU_STALE",
    "GPS_STALE",
    "CAN_INV_STALE",
    "CAN_BMS_STALE",
    "CAN_VCU_STALE",
    "SD_INIT_FAIL",
    "SD_OPEN_FAIL",
    "SD_WRITE_FAIL",
    "IMU_INIT_FAIL",
    "CAN_BUS_OFF",
    "COOLANT_TC1_OPEN",
    "COOLANT_TC2_OPEN",
};

volatile uint32_t last_wheel_speed_ms = 0;
volatile uint32_t last_adc_ms = 0;
volatile uint32_t last_tc_ms = 0;
volatile uint32_t last_imu_ms = 0;
volatile uint32_t last_gps_ms = 0;
volatile uint32_t last_can_inv_ms = 0;
volatile uint32_t last_can_bms_ms = 0;
volatile uint32_t last_can_vcu_ms = 0;

// Only touched from loop(), never from an ISR -> no race conditions.
static uint32_t faults = 0;
static uint32_t last_logged_faults = 0;
static uint8_t stale_count[FAULT_COUNT] = {0};
static uint32_t last_check_ms = 0;

void setFault(Fault f) { faults |= (1UL << f); }
void clearFault(Fault f) { faults &= ~(1UL << f); }
bool hasFault(Fault f) { return faults & (1UL << f); }
uint32_t getFaults() { return faults; }
const char *faultName(Fault f) { return (f < FAULT_COUNT) ? FAULT_NAMES[f] : "UNKNOWN"; }

// Turns a stale fault on after FAULT_CONFIRM_COUNT bad checks in a row,
// and off as soon as fresh data shows up again
static void checkStale(Fault f, uint32_t last_ms, uint32_t timeout_ms, uint32_t now)
{
    if (now - last_ms > timeout_ms)
    { // unsigned subtraction: safe when millis() wraps
        if (stale_count[f] < FAULT_CONFIRM_COUNT)
            stale_count[f]++;
        if (stale_count[f] >= FAULT_CONFIRM_COUNT)
            setFault(f);
    }
    else
    {
        stale_count[f] = 0;
        clearFault(f);
    }
}

static void logFaultChanges(uint32_t now)
{
    uint32_t changed = faults ^ last_logged_faults;
    if (!changed)
        return;

    for (uint8_t i = 0; i < FAULT_COUNT; i++)
    {
        if (changed & (1UL << i))
        {
            Serial.print("[");
            Serial.print(now);
            Serial.print(" ms] ");
            Serial.print(faultName((Fault)i));
            Serial.println(hasFault((Fault)i) ? " ON" : " cleared");
        }
    }
    Serial.print("faults=0x");
    Serial.println(faults, HEX);
    // TODO: also write this to the SD card once the SD logger exists

    last_logged_faults = faults;
}

void checkFaults(uint32_t now)
{
    if (now < STARTUP_GRACE_MS)
        return; // sensors still booting
    if (now - last_check_ms < FAULT_CHECK_PERIOD_MS)
        return; // loop runs way faster than we need
    last_check_ms = now;

    checkStale(FAULT_WHEEL_SPEED_STALE, last_wheel_speed_ms, WHEEL_SPEED_TIMEOUT_MS, now);
    checkStale(FAULT_ADC_STALE, last_adc_ms, ADC_TIMEOUT_MS, now);
    checkStale(FAULT_TC_STALE, last_tc_ms, TC_TIMEOUT_MS, now);
    checkStale(FAULT_IMU_STALE, last_imu_ms, IMU_TIMEOUT_MS, now);
    checkStale(FAULT_GPS_STALE, last_gps_ms, GPS_TIMEOUT_MS, now);
    checkStale(FAULT_CAN_INV_STALE, last_can_inv_ms, CAN_INV_TIMEOUT_MS, now);
    checkStale(FAULT_CAN_BMS_STALE, last_can_bms_ms, CAN_BMS_TIMEOUT_MS, now);
    checkStale(FAULT_CAN_VCU_STALE, last_can_vcu_ms, CAN_VCU_TIMEOUT_MS, now);

    logFaultChanges(now);
}