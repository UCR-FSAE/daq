
#include <Arduino.h>
#include "WheelSpeed.h"

// current_time_ms is always an up-to-date variable storing the elapsed time since startup in milliseconds
// USE CURRENT_TIME_MS FOR INTERUPT TIMESTAMPS AND DATA SENDING RATES
elapsedMillis current_time_ms;

void setup() {
    WheelSpeed::begin();
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
}

void loop() {
    if (WheelSpeed::update()) {

        const auto& ws = WheelSpeed::getData();

        // refer to WheelSpeed.h for RPM/Hz data calls. Example read shown below:
        // do NOT use Serial for any timing operations, as it can potentially 
        //introduce delays and affect the accuracy of wheel speed measurements.
        Serial.printf(
            "FL: %.1f | FR: %.1f | RL: %.1f | RR: %.1f RPM\n",
            ws.rpm[WheelSpeed::FL],
            ws.rpm[WheelSpeed::FR],
            ws.rpm[WheelSpeed::RL],
            ws.rpm[WheelSpeed::RR]
        );
    }
}
