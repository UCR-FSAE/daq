#pragma once
#include <Arduino.h>
#include "CanParser.h"

#define TELEM_SERIAL  Serial2
#define TELEM_BAUD    57600

void telemInit();
void telemSend(const CanParser& can);