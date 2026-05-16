#pragma once
#include <Arduino.h>

#define TELEM_SERIAL  Serial2
#define TELEM_BAUD    57600

struct logPacket {
  uint32_t time_ms;
  float temp_1;
  float temp_3;
  int16_t motor_speed;
  float phaseACurrent;
  float phaseBCurrent;
  float phaseCCurrent;
  float dcBusCurrent;
  float dcBusVoltage;
  float outputVoltage;
  float system12V;
  uint8_t inverterState;
  bool inverterEnableLockout;
  uint16_t postFaultLo;
  uint16_t postFaultHi;
  uint16_t runFaultLo;
  uint16_t runFaultHi;
  float commandedTorque;
  float torqueFeedback;
  uint32_t powerOnTimerCounts;
  float powerOnTimerSeconds;
  long lat;
  long lon;
};

void telemInit();
void telemSend(const logPacket& pkt);