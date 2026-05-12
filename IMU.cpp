#include "IMU.h"

void IMU::init() {
  Wire2.begin(); // Wire2 should correspond to SCL 24 and SDA 25

  if (_imu.begin() != ASM330LHH_OK) {
    Serial.println("IMU init failed");
  } else {
    _imu.Enable_X(); // accelerometer xyz
    _imu.Enable_G(); // gyro
  }
}

IMU_Data IMU::read() {  // Reminder to ask justin: do we want interupt driven data reading? Pin is on the pinout, but is unused in my logic
  _imu.Get_X_Axes(_data.accel);
  _imu.Get_G_Axes(_data.gyro);

  return _data;
}
