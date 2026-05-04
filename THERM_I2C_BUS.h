#ifndef THERM_I2C_BUS_H
#define THERM_I2C_BUS_H

#include <Adafruit_MCP9600.h>
#include <Wire.h>

class THERM_I2C_BUS {
 private:
  Adafruit_MCP9600 devices[6];

  uint8_t device_addresses[6] = {
    0b1100001,
    0b1100010,
    0b1100011,
    0b1100100,
    0b1100101,
    0b1100110
  };

  float readings[6];

 public:
  THERM_I2C_BUS();
  float* read_thermals();
};

#endif // THERM_I2C_BUS_H
