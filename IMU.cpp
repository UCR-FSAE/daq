#include "IMU.h"

void IMU::init() {
  Wire2.begin();

  if (_imu.begin() != ASM330LHH_OK) {
    Serial.println("IMU init failed");  //  I am unsure if printing errors to serial is how we want to debug, keeping a note here to ask
    while (1); //                           Also unsure if freezing the program is a good idea here or maybe just disabling IMU functionality
  }

  _imu.Enable_X(); // accelerometer xyz
  _imu.Enable_G(); // gyro
}

IMU_Data IMU::read() {
  _imu.Get_X_Axes(_data.accel);
  _imu.Get_G_Axes(_data.gyro);

  return _data;
}