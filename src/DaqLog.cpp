// Glue between the sensors, the fault tracker (faults.h) and the SD logger.
// main.cpp only calls sdLoggerBegin() in setup() and logSensorsToSD() in loop().
#include <Wire.h>
#include "SensorSamples.h"
#include "SensorHealth.h"
#include "SDLogger.h"
#include "LogConfig.h"
#include "faults.h"

#ifndef DAQLOG_SERIAL_STATUS
#define DAQLOG_SERIAL_STATUS 1     // print the 1 Hz status line on USB Serial
#endif

// ---------------------------------------------------------------------------
// Hot-plug probes. Weak so the bench env (src/bench/bench_fakes.cpp) can
// replace them with fake devices.
// ---------------------------------------------------------------------------

// IMU: ICM-20948 on Wire2 (SDA 25 / SCL 24), WHO_AM_I register 0x00 reads 0xEA,
// address 0x69 (AD0 high) or 0x68 (same as WILL/IMU). Probe only; once WILL/IMU is
// merged, imuProbe() should call IMU::begin() and imuPoll() IMU::update() + fill imuSample.
static bool readWhoAmI(uint8_t addr, uint8_t& v) {
    Wire2.beginTransmission(addr);
    Wire2.write((uint8_t)0x00);
    if (Wire2.endTransmission(false) != 0) return false;
    if (Wire2.requestFrom(addr, (uint8_t)1) != 1) return false;
    v = Wire2.read();
    return true;
}
__attribute__((weak)) bool imuProbe() {
    static bool wireStarted = false;
    if (!wireStarted) { Wire2.setSDA(25); Wire2.setSCL(24); Wire2.begin(); Wire2.setClock(400000); wireStarted = true; }
    uint8_t v;
    return (readWhoAmI(0x69, v) && v == 0xEA) || (readWhoAmI(0x68, v) && v == 0xEA);
}
__attribute__((weak)) void imuPoll() {}            // real driver fills imuSample here

// LoRa radio (Elias/wireless-telemetry, Serial2 57600): a transparent serial radio can't be
// detected without a reply protocol, so the default probe says "absent". Replace once the
// radio's AUX/AT handshake is known.
__attribute__((weak)) bool loraProbe() { return false; }
__attribute__((weak)) void loraSendAlert(const char* msg) { (void)msg; }   // telemetry module: telemSendString(msg)

// ---------------------------------------------------------------------------
// Fault tracker -> SD log (same writeRecord() path as sensor data)
// ---------------------------------------------------------------------------
static uint8_t loraIdx = 0xFF;

static void faultRecordToSd(FaultRecord type, uint32_t time_ms, const void* data, uint8_t len) {
    const uint8_t id = (type == FREC_DAQ_FAULTS) ? REC_FAULT_DAQ : REC_FAULT_INV;
    SDLogger::writeRecord(id, time_ms, data, len);   // a refusal is counted in SDLogger stats
}

static void pitAlert(Fault f, bool active) {
    const bool loraUp = loraIdx != 0xFF && (SensorHealth::presentMask() & (1u << loraIdx));
    if (loraUp) {
        char msg[40];
        snprintf(msg, sizeof(msg), "ALERT,%s,%s", faultName(f), active ? "ON" : "OFF");
        loraSendAlert(msg);
    }
    const PitAlertData d = {(uint8_t)f, (uint8_t)active, (uint8_t)loraUp};
    SDLogger::writeRecord(REC_PIT_ALERT, current_time_ms, &d, sizeof(d));
}

static const char* faultNames[FAULT_COUNT];
static SDLogger::HeaderInfo hdr;

// Fault recovery for the SD card: retry once a second (Brian's RECOVER_PERIOD_MS),
// like CAN-Insight's 1 s SD retry. Each retry that succeeds opens the next part.
static void sdRecover() { SDLogger::begin(hdr); }

static void registerSensors() {
    //                 name         timestamp                          timeout  probe      poll
    SensorHealth::add("car_speed",  &carSpeedSample.timestampMs,       100);
    SensorHealth::add("imu",        &imuSample.timestampMs,            50,      imuProbe,  imuPoll);
    SensorHealth::add("brake_temp", &brakeTempSample.timestampMs,      500);
    SensorHealth::add("shock_pot",  &shockPotSample.timestampMs,       50);
    SensorHealth::add("steering",   &steeringSample.timestampMs,       50);
    SensorHealth::add("gps",        &last_gps_ms,                      3000);
    loraIdx = SensorHealth::add("lora", nullptr,                       0,       loraProbe);
}

bool sdLoggerBegin() {
    registerSensors();
    for (uint8_t i = 0; i < FAULT_COUNT; i++) faultNames[i] = faultName((Fault)i);
    setLogHandler(faultRecordToSd);
    setPitAlertHandler(pitAlert);
    setRecoverHandler(FAULT_SD_INIT_FAIL, sdRecover);
    setRecoverHandler(FAULT_SD_OPEN_FAIL, sdRecover);
    setRecoverHandler(FAULT_SD_WRITE_FAIL, sdRecover);
    hdr = {kLogTypes, kNumLogTypes, SensorHealth::names(), SensorHealth::count(), faultNames, FAULT_COUNT};
    return SDLogger::begin(hdr);
}

// SD state -> Brian's SD faults. Called from loop() only (setFault rule).
static void updateSdFaults() {
    static uint32_t lastErrors = 0;
    const SDLogger::Stats s = SDLogger::stats();
    const bool newErrors = s.writeErrors > lastErrors;
    lastErrors = s.writeErrors;
    if (s.ok) {
        clearFault(FAULT_SD_INIT_FAIL); clearFault(FAULT_SD_OPEN_FAIL);
        if (newErrors) setFault(FAULT_SD_WRITE_FAIL); else clearFault(FAULT_SD_WRITE_FAIL);
    } else if (newErrors || hasFault(FAULT_SD_WRITE_FAIL)) {
        setFault(FAULT_SD_WRITE_FAIL);               // died mid-run; sdRecover() retries every 1 s
    } else if (!SDLogger::cardPresent()) {
        setFault(FAULT_SD_INIT_FAIL);                // no card / card not answering
    } else {
        setFault(FAULT_SD_OPEN_FAIL);                // card ok but the part could not be created
    }
    if (s.full) setFault(FAULT_SD_FULL); else clearFault(FAULT_SD_FULL);
}

static void writeStatus(uint32_t now) {
    static uint16_t seq = 0;
    const SDLogger::Stats s = SDLogger::stats();
    StatusData d{};
    d.seq = seq++;
    d.presentMask = SensorHealth::presentMask();
    d.aliveMask = SensorHealth::aliveMask();
    d.sdFreeMB = s.freeMB; d.writeErrors = s.writeErrors;
    d.droppedBytes = s.droppedBytes; d.droppedRecords = s.droppedRecords;
    d.bufMaxUsed = s.maxBufUsed; d.bufUsed = s.bufUsed; d.overruns = s.overruns;
    d.faults = getFaults(); d.filePart = s.part; d.fileBytes = s.fileBytes; d.maxServiceUs = s.maxServiceUs;
    SDLogger::writeRecord(REC_STATUS, now, &d, sizeof(d));

#if DAQLOG_SERIAL_STATUS
    // e.g. STATUS t=12000 sd=ok boot=3 part=0 free=3712MB buf=1024/65536 drop=0 err=0 faults=0x00000008 | car_speed=OK imu=STALE gps=ABSENT ...
    if (Serial) {
        Serial.printf("STATUS t=%lu sd=%s boot=%lu part=%u free=%ldMB buf=%lu/%lu max=%lu drop=%lu err=%lu svc=%luus prealloc=%lums faults=0x%08lX |",
                      (unsigned long)now, s.ok ? "ok" : (s.full ? "FULL" : "DOWN"), (unsigned long)s.bootNo, s.part,
                      s.freeMB == 0xFFFFFFFFUL ? -1L : (long)s.freeMB, (unsigned long)s.bufUsed,
                      (unsigned long)SDLogger::RING_BUF_SIZE, (unsigned long)s.maxBufUsed, (unsigned long)s.droppedBytes,
                      (unsigned long)s.writeErrors, (unsigned long)s.maxServiceUs, (unsigned long)s.lastPreallocMs, (unsigned long)d.faults);
        for (uint8_t i = 0; i < SensorHealth::count(); i++) {
            const bool p = d.presentMask & (1u << i), a = d.aliveMask & (1u << i);
            Serial.printf(" %s=%s", SensorHealth::names()[i], a ? "OK" : (p ? "STALE" : "ABSENT"));
        }
        Serial.println();
    }
#endif
}

void logSensorsToSD() {
    const uint32_t now = current_time_ms;
    SensorHealth::service(now);                            // probe/poll hot-plug sensors, alive/stale

    // Bridge for Brian's stale check until drivers set last_imu_ms themselves.
    if (imuSample.newSample) last_imu_ms = imuSample.timestampMs;

    SDLogger::logIfNew(REC_CAR_SPEED,  carSpeedSample);   // writes only if flag set, then clears
    SDLogger::logIfNew(REC_IMU,        imuSample);
    SDLogger::logIfNew(REC_BRAKE_TEMP, brakeTempSample);
    SDLogger::logIfNew(REC_SHOCK_POT,  shockPotSample);
    SDLogger::logIfNew(REC_STEERING,   steeringSample);

    static uint32_t lastStatus = 0;
    if (now - lastStatus >= 1000) { lastStatus = now; updateSdFaults(); writeStatus(now); }

    SDLogger::service();                                   // one sector write if card isn't busy
}
