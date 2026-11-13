#include "SensorSamples.h"
#include "SDLogger.h"
#include "LogConfig.h"

SensorSample<CarSpeedData>  carSpeedSample;
SensorSample<ImuData>       imuSample;
SensorSample<BrakeTempData> brakeTempSample;
SensorSample<ShockPotData>  shockPotSample;
SensorSample<SteeringData>  steeringSample;

bool sdLoggerBegin() {
    return SDLogger::begin(kLogHeader);
}

void logSensorsToSD() {
    SDLogger::logIfNew(REC_CAR_SPEED,  carSpeedSample);   // writes only if flag set, then clears
    SDLogger::logIfNew(REC_IMU,        imuSample);
    SDLogger::logIfNew(REC_BRAKE_TEMP, brakeTempSample);
    SDLogger::logIfNew(REC_SHOCK_POT,  shockPotSample);
    SDLogger::logIfNew(REC_STEERING,   steeringSample);
    SDLogger::service();                                  // one sector write if card isn't busy
}
