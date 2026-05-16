#pragma once
#include <Arduino.h>


#define TELEM_SERIAL  Serial3
#define TELEM_BAUD    9600

void telemInit();
void telemSend(unsigned long elapsed,
               double ambientC,
               double inletC,   bool inletOk,
               double outletC,  bool outletOk,
               float  hz,       float lpm);
