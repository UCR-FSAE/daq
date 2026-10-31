#ifndef GPS_H
#define GPS_H

#include <Arduino.h>
#include <TinyGPS.h>
#include <elapsedMillis.h>

// GPS things
#define GPS_SERIAL  Serial8
#define GPS_BAUD    9600
#define GPS_DELAY   123  // (in miliseconds) should be to the order of ~200

namespace gps {

struct Measurement {
    unsigned long age; // ms since string parsed(?)
    unsigned long daq_ms; // syncs gps_time to daq time using the GPS_DELAY
    float speed = 0.0; // speed mph(?)
};

void gpsInit(elapsedMillis*);
bool update();
// bool update(IMU accel);  // TODO: Acceleration data only rated for < 4g
Measurement& getData();

}  // namespace gps

// helper
uint32_t utcToMs(uint32_t);

#endif // GPS_H