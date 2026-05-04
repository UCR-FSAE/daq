#include "THERM_I2C_BUS.h"

// SCL_THERM_PIN =     19;
// SDA_THERM_PIN =     18;

THERM_I2C_BUS::THERM_I2C_BUS(){
  Wire.begin();
  for (int i = 0; i < 6; i++) {
    devices[i].begin(device_addresses[i]);
    // devices[i].setThermocoupleType(MCP9600_TYPE_X); FIXME: verify device type
  }
}

float* THERM_I2C_BUS::read_thermals() {
  for (int i = 0; i < 6; i++) {
    readings[i] = devices[i].readThermocouple();
  }
  return readings;
}