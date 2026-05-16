#include "HardwareSerial.h"
#include "TELEM.h"

// sends
void radio::send(float message) const {
  _port.print(message);
}

void radio::send(uint16_t message) const {
  _port.print(message);
}

void radio::send(uint32_t message) const {
  _port.print(message);
}

void radio::send(bool message) const {
  if (message) {
    _port.print("true");
  } else {
    _port.print("false");
  }
}

void radio::send(const char* message) const {
  _port.print(message);
}

// send with newlines
void radio::sendln(float message) const {
  _port.println(message);
}

void radio::sendln(uint16_t message) const {
  _port.println(message);
}

void radio::sendln(uint32_t message) const {
  _port.println(message);
}

void radio::sendln(bool message) const {
  if (message) {
    _port.println("true");
  } else {
    _port.println("false");
  }
}

void radio::sendln(const char* message) const {
  _port.println(message);
}
