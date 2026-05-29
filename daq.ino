#include <SPI.h>
#include <math.h>
#include <cstddef>
// telemetry process

// gps process

// can process

// sd card process

// therm process: MCP96RL00

// adc process: ADS8688IDBTR
const short commandByte = 
[0B1100000000000000, 0B1100010000000000, 0B1100100000000000, 0B1100110000000000];
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
  constexpr uint8_t ADC_SCLK_PIN =      13;
  constexpr uint8_t ADC_SDI_PIN =       11; // MOSI
  constexpr uint8_t ADC_SDO_PIN =       12; // MISO
  constexpr uint8_t CS_PIN =            10;
  constexpr uint8_t RST_PIN =           1;

  // serial writing
  serial.begin(115200);

  // Initialize SPI pins
  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);

  SPI.begin(ADC_SCLK_PIN, ADC_SDO_PIN, ADC_SDI_PIN, CS_PIN);
}

int readShockPot(int channel) {
  digitalWrite(CS_PIN, LOW);

  // Send start bits to activate channel
  SPI.transfer16(commandByte[channel]);

  // Clock out dummy bytes (0x00) while reading return data.
  // Assumes that device responds with 2 bytes after the command.
  short recievedData = SPI.transfer16(0x0000);

  digitalWrite (CS_PIN, HIGH);

  return recievedData;
}

void loop() {
  // put your main code here, to run repeatedly:

  // collect adc data from pinouts
  short shockpot[4] = {};
  for (unsigned i = 0; i < 4; i++) {
    shockpot[i] = readShockPot(i);

    // Testing purposes only
    Serial.print("Raw shock pot input ");
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.print(shockpot[i]);
    Serial.print('\n');
  }

  delay(1000);
  // process thermistor data

  // process can data from pinouts
  
  // process gps data

  // telemetry process

  // write to sd card
}
