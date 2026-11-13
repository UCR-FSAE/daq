#include "SensorSamples.h"

// One latest-sample slot per sensor (see SensorSamples.h).
SensorSample<CarSpeedData>  carSpeedSample;
SensorSample<ImuData>       imuSample;
SensorSample<BrakeTempData> brakeTempSample;
SensorSample<ShockPotData>  shockPotSample;
SensorSample<SteeringData>  steeringSample;
