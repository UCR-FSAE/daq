#pragma once
#include <Arduino.h>

#define TELEM_SERIAL  Serial2 // Serial2 should corrospond to rx 7 and tx 8
#define TELEM_BAUD    57600

void telemInit();
void telemSend(unsigned long elapsed,
               double ambientC,
               double inletC,   bool inletOk,
               double outletC,  bool outletOk,
               float  hz,       float lpm);
