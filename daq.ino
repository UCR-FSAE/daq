// telemetry process

// gps process

// can process

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
  constexpr uint8_t ADC_SCLK_PIN        13;
  constexpr uint8_t ADC_SDI_PIN         11; // MOSI
  constexpr uint8_t ADC_SDO_PIN =       12; // MISO
  constexpr uint8_t CS_PIN =            10;
  constexpr uint8_t RST_PIN =           1;

  // serial writing
  serial.begin(115200);
  can.begin();
  // telemInit();
  Serial2.begin(57600);
}

void loop() {
  //static bool led_on = false;
  // put your main code here, to run repeatedly:

  // collect adc data from pinouts

  // process thermistor data

  // process can data from pinouts
  // CAN PARSER
  //can.parse_message();
  
  // process gps data

  // telemetry process

  // telemetry send filtering, telem sends at 57600 baud but teensy runs faster
  // static uint32_t last_send_time = 0;
  // uint32_t curr_time = millis();
  // telemSend();
  Serial2.println("hi");
  // if (curr_time - last_send_time >= 100) {  // 100 is 10 Hz
  //   telemSend(can.highest, can.motorTemp, can.motorSpeed,
  //             can.phaseACurrent, can.phaseBCurrent, can.phaseCCurrent,
  //             can.dcBusCurrent, can.dcBusVoltage, can.outputVoltage,
  //             can.system12V, can.inverterState, can.inverterEnableLockout,
  //             can.postFaultLo, can.postFaultHi, can.runFaultLo, can.runFaultHi,
  //             can.commandedTorque, can.torqueFeedback, can.powerOnTimerCounts,
  //             can.powerOnTimerSeconds);
  //   last_send_time = curr_time;
    
  //   led_on = !led_on;
  //   digitalWrite(LED_BUILTIN, led_on ? HIGH : LOW);
    delay(500);
  // }
>>>>>>> Stashed changes

  // write to sd card
}
