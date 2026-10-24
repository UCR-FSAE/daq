#pragma once
#include <Arduino.h>
#include <elapsedMillis.h>
#include "CanParser.h"

#define TELEM_SERIAL  Serial2
#define TELEM_BAUD    57600

void telemInit();
void telemSend(const CanParser& can, elapsedMillis time, uint32_t& last_time);
void telemSendString(const char* s);