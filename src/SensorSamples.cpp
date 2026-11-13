#include "SensorSamples.h"
#include "SDLogger.h"
#include "LogConfig.h"
#include "faults.h"

SensorSample<CarSpeedData>  carSpeedSample;
SensorSample<ImuData>       imuSample;
SensorSample<BrakeTempData> brakeTempSample;
SensorSample<ShockPotData>  shockPotSample;
SensorSample<SteeringData>  steeringSample;

// Brian's fault tracker hands its records here; they go into the same log
// through the same writeRecord() path as sensor data.
static void faultRecordToSd(FaultRecord type, uint32_t time_ms, const void* data, uint8_t len) {
    const uint8_t id = (type == FREC_DAQ_FAULTS) ? REC_FAULT_DAQ : REC_FAULT_INV;
    if (!SDLogger::writeRecord(id, time_ms, data, len)) { /* counted in SDLogger stats */ }
}

static const char* faultNames[FAULT_COUNT];

bool sdLoggerBegin() {
    for (uint8_t i = 0; i < FAULT_COUNT; i++) faultNames[i] = faultName((Fault)i);
    setLogHandler(faultRecordToSd);
    const SDLogger::HeaderInfo hdr = {kLogTypes, kNumLogTypes, nullptr, 0, faultNames, FAULT_COUNT};
    return SDLogger::begin(hdr);
}

void logSensorsToSD() {
    SDLogger::logIfNew(REC_CAR_SPEED,  carSpeedSample);   // writes only if flag set, then clears
    SDLogger::logIfNew(REC_IMU,        imuSample);
    SDLogger::logIfNew(REC_BRAKE_TEMP, brakeTempSample);
    SDLogger::logIfNew(REC_SHOCK_POT,  shockPotSample);
    SDLogger::logIfNew(REC_STEERING,   steeringSample);
    SDLogger::service();                                  // one sector write if card isn't busy
}
