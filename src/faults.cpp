#include "faults.h"
#include <string.h> // memcmp

// ---------------------------------------------------------------------------
// Sensor timeouts are placeholders until sample rates are known.
// Set timeout to 3–5x the normal sample interval.
// Max detection time = timeout + FAULT_CONFIRM_COUNT * FAULT_CHECK_PERIOD_MS.
// ---------------------------------------------------------------------------
static constexpr uint32_t FAULT_CHECK_PERIOD_MS = 10; // run checks at 100 Hz
static constexpr uint8_t FAULT_CONFIRM_COUNT = 3;     // 3 bad checks in a row = real fault
static constexpr uint32_t STARTUP_GRACE_MS = 2000;    // let sensors boot before checking
static constexpr uint32_t RECOVER_PERIOD_MS = 1000;   // retry broken stuff once a second

static constexpr uint32_t ADC_TIMEOUT_MS = 50;
static constexpr uint32_t TC_TIMEOUT_MS = 500; // temps are slow
static constexpr uint32_t IMU_TIMEOUT_MS = 50;
static constexpr uint32_t GPS_TIMEOUT_MS = 3000; // NEO-M8N default 1 Hz
// Max detection time: 70 ms + (3 × 10 ms) = 100 ms. Meets VCU's 100 ms rules (T.4.2.5, T.4.3.3).
static constexpr uint32_t CAN_INV_TIMEOUT_MS = 70;
static constexpr uint32_t CAN_BMS_TIMEOUT_MS = 70;
static constexpr uint32_t CAN_VCU_TIMEOUT_MS = 70;

// ---------------------------------------------------------------------------
// 1 row per fault, in the same order as the Fault enum. 
// Log only = ACT_LOG (+ ACT_INVALID); Act = RECOVER/ALERT.
// Keep actions synced with the FAULT SHEET.
// ---------------------------------------------------------------------------
struct FaultInfo {
    const char *name;
    uint8_t actions;
};

static const FaultInfo FAULT_INFO[] = {
    // stale data
    {"ADC_STALE", ACT_LOG | ACT_INVALID},
    {"TC_STALE", ACT_LOG | ACT_INVALID | ACT_RECOVER},
    {"IMU_STALE", ACT_LOG | ACT_INVALID | ACT_RECOVER},
    {"GPS_STALE", ACT_LOG | ACT_INVALID},
    {"CAN_INV_STALE", ACT_LOG | ACT_INVALID | ACT_ALERT_PIT}, // silent inverter/BMS/VCU
    {"CAN_BMS_STALE", ACT_LOG | ACT_INVALID | ACT_ALERT_PIT}, // probably means the car
    {"CAN_VCU_STALE", ACT_LOG | ACT_INVALID | ACT_ALERT_PIT}, // is down
    // event faults
    {"SD_INIT_FAIL", ACT_LOG | ACT_RECOVER | ACT_ALERT_PIT}, // no SD = LoRa is the only
    {"SD_OPEN_FAIL", ACT_LOG | ACT_RECOVER | ACT_ALERT_PIT}, // way anyone finds out
    {"SD_WRITE_FAIL", ACT_LOG | ACT_RECOVER | ACT_ALERT_PIT},
    {"IMU_INIT_FAIL", ACT_LOG | ACT_INVALID | ACT_RECOVER},
    {"TC_INIT_FAIL", ACT_LOG | ACT_INVALID | ACT_RECOVER},
    {"CAN_BUS_OFF", ACT_LOG | ACT_RECOVER | ACT_ALERT_PIT},
};
// Compile error if a fault is missing from this table.
static_assert(sizeof(FAULT_INFO) / sizeof(FAULT_INFO[0]) == FAULT_COUNT, "FAULT_INFO must have exactly 1 row per Fault");

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
static LogFn log_handler = nullptr;

void setFault(Fault f) { faults |= (1UL << f); }
void clearFault(Fault f) { faults &= ~(1UL << f); }
bool hasFault(Fault f) { return faults & (1UL << f); }
uint32_t getFaults() { return faults; }
const char *faultName(Fault f) { return (f < FAULT_COUNT) ? FAULT_INFO[f].name : "UNKNOWN"; }
uint8_t faultActions(Fault f) { return (f < FAULT_COUNT) ? FAULT_INFO[f].actions : 0; }
bool isLogOnly(Fault f) { return !(faultActions(f) & (ACT_RECOVER | ACT_ALERT_PIT | ACT_ALERT_DRIVER)); }
bool dataValid(Fault f) { return !(hasFault(f) && (faultActions(f) & ACT_INVALID)); }

void setRecoverHandler(Fault f, RecoverFn fn) {
    if (f < FAULT_COUNT)
        recover_handlers[f] = fn;
}
void setPitAlertHandler(AlertFn fn) { pit_alert = fn; }
void setDriverAlertHandler(AlertFn fn) { driver_alert = fn; }
void setLogHandler(LogFn fn) { log_handler = fn; }

// Hands a record to the SD logger (if one is plugged in).
static void saveRecord(FaultRecord type, uint32_t now, const void *data, uint8_t len) {
    if (log_handler)
        log_handler(type, now, data, len);
}

// ---------------------------------------------------------------------------
// Show faults in table on Serial. Print header on connection and every 20 rows
// TIME  FAULT       STATE  TYPE  ALL FAULTS
// 4080  IMU_STALE   ON     act   0x00000004
// ---------------------------------------------------------------------------
static constexpr uint8_t HEADER_ROWS = 20;
static uint8_t rows_since_header = HEADER_ROWS; // header before the 1st row
static bool monitor_was_open = false;

static void printPadded(const char *s, uint8_t width) {
    Serial.print(s);
    for (size_t n = strlen(s); n < width; n++)
        Serial.print(' ');
}

static void printRight(uint32_t v, uint8_t width) {
    uint8_t digits = 1;
    for (uint32_t x = v; x >= 10; x /= 10)
        digits++;
    for (; digits < width; digits++)
        Serial.print(' ');
    Serial.print(v);
}

static void printHex8(uint32_t v) {
    Serial.print("0x");
    for (int shift = 28; shift >= 0; shift -= 4)
        Serial.print("0123456789ABCDEF"[(v >> shift) & 0xF]);
}

static void printHeader() {
    Serial.println("    TIME  FAULT            STATE    TYPE      ALL FAULTS");
    rows_since_header = 0;
    monitor_was_open = (bool)Serial;
}

static void headerOnMonitorConnect() {
    bool open = (bool)Serial;
    if (open && !monitor_was_open)
        printHeader();
    monitor_was_open = open;
}

static void startRow(uint32_t now) {
    if (rows_since_header >= HEADER_ROWS)
        printHeader();
    rows_since_header++;
    printRight(now, 8);
    Serial.print("  ");
}

// ---------------------------------------------------------------------------
// Log 8 bytes of CAN 0x0AB when they change. Decode 54 fault bits later when reading log.
// ---------------------------------------------------------------------------
struct __attribute__((packed)) InverterFaults {
    uint16_t postFaultLo, postFaultHi, runFaultLo, runFaultHi;
};
static_assert(sizeof(InverterFaults) == 8, "0x0AB is 8 bytes");

static InverterFaults last_inv_faults = {0, 0, 0, 0};
static bool inv_faults_logged_once = false;

void logInverterFaults(uint16_t postFaultLo, uint16_t postFaultHi, uint16_t runFaultLo, uint16_t runFaultHi, uint32_t now) {
    InverterFaults cur = {postFaultLo, postFaultHi, runFaultLo, runFaultHi};
    if (inv_faults_logged_once && memcmp(&cur, &last_inv_faults, sizeof(cur)) == 0)
        return; // nothing changed, nothing to log

    startRow(now);
    printPadded("INVERTER", 17);
    printPadded("changed", 9);
    printPadded("raw", 10);
    Serial.print("post=");
    printHex8(((uint32_t)postFaultHi << 16) | postFaultLo);
    Serial.print(" run=");
    printHex8(((uint32_t)runFaultHi << 16) | runFaultLo);
    Serial.println();

    saveRecord(FREC_INV_FAULTS, now, &cur, sizeof(cur));
    last_inv_faults = cur;
    inv_faults_logged_once = true;
}

// Set stale fault after FAULT_CONFIRM_COUNT failed checks. Clear it when fresh data arrives.
static void checkStale(Fault f, uint32_t last_ms, uint32_t timeout_ms, uint32_t now) {
    if (now - last_ms > timeout_ms) { // unsigned subtraction: safe when millis() wraps
        if (stale_count[f] < FAULT_CONFIRM_COUNT)
            stale_count[f]++;
        if (stale_count[f] >= FAULT_CONFIRM_COUNT)
            setFault(f);
    }
    else {
        stale_count[f] = 0;
        clearFault(f);
    }
}

// Runs once per fault when it turns ON or OFF: log + alerts.
static void handleFaultChanges(uint32_t now) {
    uint32_t changed = faults ^ last_handled_faults;
    if (!changed)
        return;

    for (uint8_t i = 0; i < FAULT_COUNT; i++) {
        if (!(changed & (1UL << i)))
            continue;

        Fault f = (Fault)i;
        bool active = hasFault(f);
        uint8_t actions = faultActions(f);

        if (actions & ACT_LOG) {
            startRow(now);
            printPadded(faultName(f), 17);
            printPadded(active ? "ON" : "cleared", 9);
            printPadded(isLogOnly(f) ? "log only" : "act", 10);
            printHex8(faults);
            Serial.println();
        }
        if ((actions & ACT_ALERT_PIT) && pit_alert)
            pit_alert(f, active);
        if ((actions & ACT_ALERT_DRIVER) && driver_alert)
            driver_alert(f, active);
    }
    saveRecord(FREC_DAQ_FAULTS, now, &faults, sizeof(faults));

    last_handled_faults = faults;
}

// Call the recovery handler every second while an ACT_RECOVER fault is active.
static void runRecovery(uint32_t now) {
    if (now - last_recover_ms < RECOVER_PERIOD_MS)
        return;
    last_recover_ms = now;

    for (uint8_t i = 0; i < FAULT_COUNT; i++) {
        Fault f = (Fault)i;
        if (hasFault(f) && (faultActions(f) & ACT_RECOVER) && recover_handlers[f])
            recover_handlers[f]();
    }
}

void checkFaults(uint32_t now) {
    if (now < STARTUP_GRACE_MS)
        return; // sensors still booting
    if (now - last_check_ms < FAULT_CHECK_PERIOD_MS)
        return; // loop runs way faster than we need
    last_check_ms = now;

    headerOnMonitorConnect();
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