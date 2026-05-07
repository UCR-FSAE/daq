#include <TinyGPS.h>
#include <FlexCAN_T4.h>

//GLOBAL VARIABLES

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

// telemetry process

// gps process
TinyGPS gps;

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

// sd card process

// therm process: MCP96RL00

// adc process: ADS8688IDBTR

// imu process: ASM330LHHXTR


void setup() {
  // put your setup code here, to run once:
  // CAN RX and TX Pinouts
  constexpr uint8_t TEENSY_RX_PIN =     23;
  constexpr uint8_t TEENSY_TX_PIN =     22;

  // i2c pins
  constexpr uint8_t SCL_IMU_PIN =       24;
  constexpr uint8_t SDA_IMU_PIN =       25;
  constexpr uint8_t INT_INU_PIN =       5;
  constexpr uint8_t SCL_THERM_PIN =     19;
  constexpr uint8_t SDA_THERM_PIN =     18;

  // GPS pins
  constexpr uint8_t GPS_TX_PIN =        35;
  constexpr uint8_t GPS_RX_PIN =        34;

  // digital pins
  constexpr uint8_t DIG1_PIN =          29;
  constexpr uint8_t DIG2_PIN =          30;
  constexpr uint8_t DIG3_PIN =          31;
  constexpr uint8_t DIG4_PIN =          32;

  // TELEM pins
  constexpr uint8_t TELEM_RX_PIN =      7;
  constexpr uint8_t TELEM_TX_PIN =      8;

  // ADC SPI pins
  constexpr uint8_t ADC_SCLK_PIN    =    13;
  constexpr uint8_t ADC_SDI_PIN      =   11; // MOSI
  constexpr uint8_t ADC_SDO_PIN =       12; // MISO
  constexpr uint8_t CS_PIN =            10;
  constexpr uint8_t RST_PIN =           1;

  // serial writing
  Serial.begin(115200);
  Serial8.begin(9600); // GPS serial m10 ublox 

  // can
  Can0.begin();
  Can0.setBaudRate(500000);

}

void loop() {
  // put your main code here, to run repeatedly:

  // collect adc data from pinouts

  // process thermistor data

  // process can data from pinouts
  // CAN PARSER
  CAN_message_t msg;

  if (Can0.read(msg)) {

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
  float highest = moduleA;

  if (moduleB > highest) highest = moduleB;
  if (moduleC > highest) highest = moduleC;
  if (gateDriver > highest) highest = gateDriver;

  // test printing can parser output

  Serial.print("Highest selected temp (Temperatures #1): ");
  Serial.print(highest);
  Serial.println(" C");

  Serial.print("Motor Temperature: ");
  Serial.print(motorTemp);
  Serial.println(" C");
  
  //GPS
  // process gps data
  while(Serial8.available()) {
    char c = Serial8.read();
    if(gps.encode(c)) {
      long lat, lon;
      unsigned long age; //outputs in ms

      gps.get_position(&lat, &lon, &age);

      if (age != TinyGPS::GPS_INVALID_AGE) {
        Serial.print("Lat: ");
        Serial.println(lat / 1000000.0, 6);

        Serial.print("Lon: ");
        Serial.println(lon / 1000000.0, 6);
      }
    }
  }

  // telemetry process

  // write to sd card

}
