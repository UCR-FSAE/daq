#pragma once
#include <FlexCAN_T4.h>

struct CanParser {
  // Data To Collect for CAN
  // 0x0A0 – Temperatures #1 - All Info Here
  // 0x0A2 – Temperatures #3 & Torque Shudder - Motor Temp Only
  // 0x0A5 – Motor Position Information - Motor Speed Only
  // 0x0A6 – Current Information - All Info Here
  // 0x0A7 – Voltage Information - DC Bus Voltage and Output Voltage
  // 0x0A9 – Internal Voltages - 12V System voltage Only
  // 0x0AA – Internal States - Inverter State,  Inverter Enable Lockout
  // 0x0AB – Fault Codes - All Info here
  // 0x0AC – Torque & Timer Information - All Info here

  // Data to Transmit for CAN
  // 0x0A0 – Temperatures #1 - Pick the peak value out of the group transmit only that
  // 0x0A2 – Temperatures #3 & Torque Shudder - Motor Temp Only
  // 0x0A5 – Motor Position Information - Motor Speed Only
  // 0x0A6 – Current Information - DC Bus Current, Phase A Current ( Phase B/C Optional)
  // 0x0A7 – Voltage Information - DC Bus Voltage and Output Voltage
  // 0x0A9 – Internal Voltages - 12V System voltage Only
  // 0x0AA – Internal States - Inverter State,  Inverter Enable Lockout
  // 0x0AB – Fault Codes - All Info here
  // 0x0AC – Torque & Timer Information - All Info here

  // Temperatures #1: 0x0A0, TRANSMIT highest value in this section
  float moduleA = -999;
  float moduleB = -999;
  float moduleC = -999;
  float gateDriver = -999;
  float highest = moduleA;
  // Temperatures #3: 0x0A2
  float motorTemp = -999; //TRANSMIT
  // Motor Position Information: 0x0A5
  int16_t motorSpeed = 0; // RPM //TRANSMIT

  // Current Information: 0x0A6
  float phaseACurrent = 0; //TRANSMIT
  float phaseBCurrent = 0; //TRANSMIT optional
  float phaseCCurrent = 0; //TRANSMIT optional
  float dcBusCurrent = 0; //TRANSMIT

  // Voltage Information: 0x0A7
  float dcBusVoltage = 0; //TRANSMIT
  float outputVoltage = 0; //TRANSMIT

  // Internal Voltages: 0x0A9
  float system12V = 0; //TRANSMIT

  // Internal States: 0x0AA
  uint8_t inverterState = 0; //TRANSMIT
  bool inverterEnableLockout = false; //TRANSMIT

  // Fault Codes: 0x0AB
  uint16_t postFaultLo = 0; //TRANSMIT
  uint16_t postFaultHi = 0; //TRANSMIT
  uint16_t runFaultLo = 0; //TRANSMIT
  uint16_t runFaultHi = 0; //TRANSMIT

  // Torque & Timer Information: 0x0AC
  float commandedTorque = 0; //TRANSMIT
  float torqueFeedback = 0; //TRANSMIT
  uint32_t powerOnTimerCounts = 0; //TRANSMIT
  float powerOnTimerSeconds = 0; //TRANSMIT

  // can process
  FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> Can0;

  //helper functions for can parsing
// =========================
// CAN Helper Functions
// =========================

// "index" = starting byte position inside msg.buf[]
//
// Example CAN payload:
// msg.buf[0] msg.buf[1] msg.buf[2] msg.buf[3] ...
//
// index = 0 -> reads bytes 0 and 1
// index = 2 -> reads bytes 2 and 3
// index = 4 -> reads bytes 4 and 5
// index = 6 -> reads bytes 6 and 7
//
// Used because most Cascadia signals are 16-bit values
// spread across 2 bytes in little-endian format.
int16_t readS16LE(const uint8_t *buf, uint8_t index) {
  return (int16_t)(((uint16_t)buf[index + 1] << 8) | buf[index]);
}

float decodeTemp(const uint8_t *buf, uint8_t index) {
  return readS16LE(buf, index) / 10.0;
}

uint16_t readU16LE(const uint8_t *buf, uint8_t index) {
  return ((uint16_t)buf[index + 1] << 8) | buf[index];
}

uint32_t readU32LE(const uint8_t *buf, uint8_t index) {
  return ((uint32_t)buf[index + 3] << 24) |
         ((uint32_t)buf[index + 2] << 16) |
         ((uint32_t)buf[index + 1] << 8)  |
         buf[index];
}

float decodeCurrent(const uint8_t *buf, uint8_t index) {
  return readS16LE(buf, index) / 10.0;
}

float decodeHighVoltage(const uint8_t *buf, uint8_t index) {
  return readS16LE(buf, index) / 10.0;
}

float decodeLowVoltage(const uint8_t *buf, uint8_t index) {
  return readS16LE(buf, index) / 100.0;
}

float decodeTorque(const uint8_t *buf, uint8_t index) {
  return readS16LE(buf, index) / 10.0;
}

int16_t decodeSpeed(const uint8_t *buf, uint8_t index) {
  return readS16LE(buf, index);
}

void begin() {
  Can0.begin();
  Can0.setBaudRate(500000);
}

void parse_message() {
  CAN_message_t msg;
  while (Can0.read(msg)) {

    if (msg.id == 0x0A0 && msg.len >= 8) {
      moduleA    = decodeTemp(msg.buf, 0);
      moduleB    = decodeTemp(msg.buf, 2);
      moduleC    = decodeTemp(msg.buf, 4);
      gateDriver = decodeTemp(msg.buf, 6);
    }

    else if (msg.id == 0x0A2 && msg.len >= 6) {
      motorTemp = decodeTemp(msg.buf, 4);
    }

    else if (msg.id == 0x0A5 && msg.len >= 4) {
      motorSpeed = decodeSpeed(msg.buf, 2);
    }

    else if (msg.id == 0x0A6 && msg.len >= 8) {
      phaseACurrent = decodeCurrent(msg.buf, 0);
      phaseBCurrent = decodeCurrent(msg.buf, 2);
      phaseCCurrent = decodeCurrent(msg.buf, 4);
      dcBusCurrent  = decodeCurrent(msg.buf, 6);
    }

    else if (msg.id == 0x0A7 && msg.len >= 4) {
      dcBusVoltage = decodeHighVoltage(msg.buf, 0);
      outputVoltage = decodeHighVoltage(msg.buf, 2);
    }

    else if (msg.id == 0x0A9 && msg.len >= 8) {
      system12V = decodeLowVoltage(msg.buf, 6);
    }

    else if (msg.id == 0x0AA && msg.len >= 7) {
      inverterState = msg.buf[2];
      inverterEnableLockout = msg.buf[6] & (1 << 7);
    }

    else if (msg.id == 0x0AB && msg.len >= 8) {
      postFaultLo = readU16LE(msg.buf, 0);
      postFaultHi = readU16LE(msg.buf, 2);
      runFaultLo  = readU16LE(msg.buf, 4);
      runFaultHi  = readU16LE(msg.buf, 6);
    }

    else if (msg.id == 0x0AC && msg.len >= 8) {
      commandedTorque = decodeTorque(msg.buf, 0);
      torqueFeedback = decodeTorque(msg.buf, 2);
      powerOnTimerCounts = readU32LE(msg.buf, 4);
      powerOnTimerSeconds = powerOnTimerCounts * 0.003;
    }
  }
  // highest for temperatures #1 section
  highest = moduleA;

  if (moduleB > highest) highest = moduleB;
  if (moduleC > highest) highest = moduleC;
  if (gateDriver > highest) highest = gateDriver;
}

};