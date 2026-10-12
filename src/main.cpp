#include <Arduino.h>
#include "CanParser.h"
#include "TELEM.h"
// See "DAQ DOC" for peripheral PNs
// See "VCU Pinouts" google sheet for pinouts

// can process
CanParser can;

void setup() {
  // put your setup code here, to run once:
  // CAN RX and TX Pinouts
  // constexpr uint8_t TEENSY_RX_PIN =     23;
  // constexpr uint8_t TEENSY_TX_PIN =     22;

  // i2c pins
  // constexpr uint8_t SCL_IMU_PIN =       24;
  // constexpr uint8_t SDA_IMU_PIN =       25;
  // constexpr uint8_t INT_INU_PIN =       5;
  // constexpr uint8_t SCL_THERM_PIN =     19;
  // constexpr uint8_t SDA_THERM_PIN =     18;

  // GPS pins
  // constexpr uint8_t GPS_TX_PIN =        35;
  // constexpr uint8_t GPS_RX_PIN =        34;

  // digital pins
  // constexpr uint8_t DIG1_PIN =          29;
  // constexpr uint8_t DIG2_PIN =          30;
  // constexpr uint8_t DIG3_PIN =          31;
  // constexpr uint8_t DIG4_PIN =          32;

  // TELEM pins
  // constexpr uint8_t TELEM_RX_PIN =      7;
  // constexpr uint8_t TELEM_TX_PIN =      8;

  // ADC SPI pins
  // constexpr uint8_t ADC_SCLK_PIN =      13;
  // constexpr uint8_t ADC_SDI_PIN =       11; // MOSI
  // constexpr uint8_t ADC_SDO_PIN =       12; // MISO
  // constexpr uint8_t CS_PIN =            10;
  // constexpr uint8_t RST_PIN =           1;

  // serial writing
  Serial.begin(115200);
  can.begin();
  delay(100);

  telemInit();
  delay(100);

  pinMode(LED_BUILTIN, OUTPUT); digitalWrite(LED_BUILTIN, HIGH);
  Serial.println("Setup done");
  delay(1000);
}

void loop() {
  // put your main code here, to run repeatedly:

  // collect adc data (4x shock pots, 1x steering pot, 1x flow meter)

  // collect thermistor data (4x brake pad temps, 2x coolant temps)

  // collect can data (CAN lol, im not writing allat)
  can.parse_message();
  
  // collect gps data (speed, time)

  // collect IMU data (longitudinal, lateral, & vertical accel as well as roll pitch & yaw rate)

  // write to telemetry
  static uint32_t last_send_time = 0;
  uint32_t curr_time = millis();
  if (curr_time - last_send_time >= 100) {
    telemSend(can);
    //Serial.println("Sent!");  // for debugging
    last_send_time = curr_time;
  }

  // write to sd card

}
