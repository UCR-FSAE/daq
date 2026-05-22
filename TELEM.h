#pragma once
#include <Arduino.h>

// #define TELEM_SERIAL  Serial
// #define TELEM_BAUD    115200

#define TELEM_SERIAL  Serial2 // Serial2 should corrospond to rx 7 and tx 8
#define TELEM_BAUD    57600

void telemInit();
void telemSend(float, float, int16_t,
               float, float, float,
               float, float, float,
               float, uint8_t, bool,
               uint16_t, uint16_t, uint16_t, uint16_t,
               float, float, uint32_t,
               float);