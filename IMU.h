#include <Wire.h>
#include <ASM330LHHSensor.h>

#ifndef IMU_H
#define IMU_H

// data type for storing IMU data:
struct IMU_Data {
  int32_t accel[3]; // [0] = x , [1] = y , [2] = z acceleration in milli g ... access with OBJECT.accel[#];
  int32_t gyro[3];  // [0] = pitch , [1] = roll , [2] = yaw angular velocity (w) in milli dps ... access with OBJECT.gyro[#];
};

// class for using an ASM330LHHXG1 6-DOF IMU
class IMU {
 private:
  IMU_Data _data;
  ASM330LHHSensor _imu;

 public:
  IMU() : _imu(&Wire2, 0x6A) {}
  void init();

  IMU_Data read();  // reads sensor data and optionally returns updated _data values
  IMU_Data quick_read() const { return _data; } // returns last read _data values (for writing to sd card)
};

#endif
