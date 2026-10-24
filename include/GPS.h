#ifndef GPS_H
#define GPS_H

#include <Arduino.h>
#include <TinyGPS.h>

// GPS serial pins
#define GPS_BAUD    Serial8
#define GPS_BAUD    9600

namespace gps {

struct Measurement {
};

void gpsInit();
bool update();
// void update(IMU accel);  // TODO: Acceleration data only rated for <4g
void getData();

}

#endif // GPS_H