#include "faults.h"

// ---------------------------------------------------------------------------
// Timing, sensor timeouts are PLACEHOLDERS until we know real sample rates.
// Rule of thumb: timeout = ~3-5x the sensor's normal time between samples.
// Worst-case detection = timeout + FAULT_CONFIRM_COUNT x FAULT_CHECK_PERIOD_MS.
// ---------------------------------------------------------------------------
static constexpr uint32_t FAULT_CHECK_PERIOD_MS = 10; // run checks at 100 Hz
static constexpr uint8_t FAULT_CONFIRM_COUNT = 3;     // 3 bad checks in a row = real fault
static constexpr uint32_t STARTUP_GRACE_MS = 2000;    // let sensors boot before checking
static constexpr uint32_t RECOVER_PERIOD_MS = 1000;   // retry broken stuff once a second

static constexpr uint32_t WHEEL_SPEED_TIMEOUT_MS = 50; // Will's module updates every 20 ms
static constexpr uint32_t ADC_TIMEOUT_MS = 50;
static constexpr uint32_t TC_TIMEOUT_MS = 500; // temps are slow
static constexpr uint32_t IMU_TIMEOUT_MS = 50;
static constexpr uint32_t GPS_TIMEOUT_MS = 1000;
// 70 ms + 3 checks x 10 ms = 100 ms worst case, matches VCU's 100 ms rules (T.4.2.5, T.4.3.3)
static constexpr uint32_t CAN_INV_TIMEOUT_MS = 70;
static constexpr uint32_t CAN_BMS_TIMEOUT_MS = 70;
static constexpr uint32_t CAN_VCU_TIMEOUT_MS = 70;

// ---------------------------------------------------------------------------
// One row per fault, SAME ORDER as the Fault enum.
// "Log only" = ACT_LOG (+ ACT_INVALID). "Act" = anything with RECOVER/ALERT.
// Keep this in sync with the "Action" column on the FAULT SHEET.
// ---------------------------------------------------------------------------
struct FaultInfo
{
    const char *name;
    uint8_t actions;
};

static const FaultInfo FAULT_INFO[] = {
    // stale data
    {"WHEEL_SPEED_STALE", ACT_LOG | ACT_INVALID},
    {"ADC_STALE",         ACT_LOG | ACT_INVALID},
    {"TC_STALE",          ACT_LOG | ACT_INVALID | ACT_RECOVER},
    {"IMU_STALE",         ACT_LOG | ACT_INVALID | ACT_RECOVER},
    {"GPS_STALE",         ACT_LOG | ACT_INVALID},
    {"CAN_INV_STALE",     ACT_LOG | ACT_INVALID | ACT_ALERT_PIT}, // silent inverter/BMS/VCU
    {"CAN_BMS_STALE",     ACT_LOG | ACT_INVALID | ACT_ALERT_PIT}, // probably means the car
    {"CAN_VCU_STALE",     ACT_LOG | ACT_INVALID | ACT_ALERT_PIT}, // is down
    // event faults
    {"SD_INIT_FAIL",      ACT_LOG | ACT_RECOVER | ACT_ALERT_PIT}, // no SD = LoRa is the only
    {"SD_OPEN_FAIL",      ACT_LOG | ACT_RECOVER | ACT_ALERT_PIT}, // way anyone finds out
    {"SD_WRITE_FAIL",     ACT_LOG | ACT_RECOVER | ACT_ALERT_PIT},
    {"IMU_INIT_FAIL",     ACT_LOG | ACT_INVALID | ACT_RECOVER},
    {"CAN_BUS_OFF",       ACT_LOG | ACT_RECOVER | ACT_ALERT_PIT},
    {"COOLANT_TC1_OPEN",  ACT_LOG | ACT_INVALID}, // broken wire, retrying won't fix it
    {"COOLANT_TC2_OPEN",  ACT_LOG | ACT_INVALID},
};
// Compile error if someone adds a fault to the enum but forgets a row here.
static_assert(sizeof(FAULT_INFO) / sizeof(FAULT_INFO[0]) == FAULT_COUNT, "FAULT_INFO must have exactly one row per Fault");

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
static uint32_t last_handled_faults = 0;
static uint8_t stale_count[FAULT_COUNT] = {0};
static uint32_t last_check_ms = 0;
static uint32_t last_recover_ms = 0;

static RecoverFn recover_handlers[FAULT_COUNT] = {nullptr};
static AlertFn pit_alert = nullptr;
static AlertFn driver_alert = nullptr;

void setFault(Fault f) { faults |= (1UL << f); }
void clearFault(Fault f) { faults &= ~(1UL << f); }
bool hasFault(Fault f) { return faults & (1UL << f); }
uint32_t getFaults() { return faults; }
const char *faultName(Fault f) { return (f < FAULT_COUNT) ? FAULT_INFO[f].name : "UNKNOWN"; }
uint8_t faultActions(Fault f) { return (f < FAULT_COUNT) ? FAULT_INFO[f].actions : 0; }
bool isLogOnly(Fault f) { return !(faultActions(f) & (ACT_RECOVER | ACT_ALERT_PIT | ACT_ALERT_DRIVER)); }
bool dataValid(Fault f) { return !(hasFault(f) && (faultActions(f) & ACT_INVALID)); }

void setRecoverHandler(Fault f, RecoverFn fn)
{
    if (f < FAULT_COUNT)
        recover_handlers[f] = fn;
}
void setPitAlertHandler(AlertFn fn) { pit_alert = fn; }
void setDriverAlertHandler(AlertFn fn) { driver_alert = fn; }

// Turns a stale fault on after FAULT_CONFIRM_COUNT bad checks in a row,
// and off as soon as fresh data shows up again
static void checkStale(Fault f, uint32_t last_ms, uint32_t timeout_ms, uint32_t now)
{
    if (now - last_ms > timeout_ms) // unsigned subtraction: safe when millis() wraps
    {
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

// Runs once per fault when it turns ON or OFF: log + alerts.
static void handleFaultChanges(uint32_t now)
{
    uint32_t changed = faults ^ last_handled_faults;
    if (!changed)
        return;

    for (uint8_t i = 0; i < FAULT_COUNT; i++)
    {
        if (!(changed & (1UL << i)))
            continue;

        Fault f = (Fault)i;
        bool active = hasFault(f);
        uint8_t actions = faultActions(f);

        if (actions & ACT_LOG)
        {
            Serial.print("[");
            Serial.print(now);
            Serial.print(" ms] ");
            Serial.print(faultName(f));
            Serial.print(active ? " ON" : " cleared");
            Serial.println(isLogOnly(f) ? " (log only)" : " (act)");
        }
        if ((actions & ACT_ALERT_PIT) && pit_alert)
            pit_alert(f, active);
        if ((actions & ACT_ALERT_DRIVER) && driver_alert)
            driver_alert(f, active);
    }
    Serial.print("faults=0x");
    Serial.println(faults, HEX);
    // TODO: write the raw uint32_t to Will's binary SD log

    last_handled_faults = faults;
}

// While a fault with ACT_RECOVER is on, call its recover handler once a second.
static void runRecovery(uint32_t now)
{
    if (now - last_recover_ms < RECOVER_PERIOD_MS)
        return;
    last_recover_ms = now;

    for (uint8_t i = 0; i < FAULT_COUNT; i++)
    {
        Fault f = (Fault)i;
        if (hasFault(f) && (faultActions(f) & ACT_RECOVER) && recover_handlers[f])
            recover_handlers[f]();
    }
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

    handleFaultChanges(now);
    runRecovery(now);
}