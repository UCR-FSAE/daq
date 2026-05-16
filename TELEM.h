#include "HardwareSerial.h"
#ifndef TELEM_H
#define TELEM_H

#include <stdint.h>
#include <Arduino.h>

class radio{
 private:
  // const variables
  const uint16_t BAUD_RATE = 57600;

  // serial port
  HardwareSerial& _port;

 public:
  radio(HardwareSerial& port) : _port(port) {}
  void init() { _port.begin(BAUD_RATE, SERIAL_8N1); }

  // send overlaods
  void send(float) const;
  void send(uint16_t) const;
  void send(uint32_t) const;
  void send(bool) const;
  void send(const char*) const;

  void sendln(float) const;
  void sendln(uint16_t) const;
  void sendln(uint32_t) const;
  void sendln(bool) const;
  void sendln(const char*) const;
};

#endif  //TELEM_H