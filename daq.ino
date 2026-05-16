// telemetry process

// gps process
TinyGPS gps;
void printPacificTime(byte hour, byte minute, byte second) {
  int pacificHour = hour - 7; // PDT. Use -8 for PST.

  if (pacificHour < 0) {
    pacificHour += 24;
  }

  String ampm = "AM";

  if (pacificHour >= 12) {
    ampm = "PM";
  }

  int displayHour = pacificHour % 12;

  if (displayHour == 0) {
    displayHour = 12;
  }

  Serial.print("Pacific Time: ");

  if (displayHour < 10) Serial.print("0");
  Serial.print(displayHour);

  Serial.print(":");

  if (minute < 10) Serial.print("0");
  Serial.print(minute);

  Serial.print(":");

  if (second < 10) Serial.print("0");
  Serial.print(second);

  Serial.print(" ");
  Serial.println(ampm);
}

// can process
CanParser can;


// sd card process

// therm process: MCP96RL00

// adc process: ADS8688IDBTR

// imu process: ASM330LHHXTR


void setup() {
  // put your setup code here, to run once:
  // CAN RX and TX Pinouts
  // constexpr uint8_t TEENSY_RX_PIN =     23;
  // constexpr uint8_t TEENSY_TX_PIN =     22;

  // // i2c pins
  // constexpr uint8_t SCL_IMU_PIN =       24;
  // constexpr uint8_t SDA_IMU_PIN =       25;
  // constexpr uint8_t INT_INU_PIN =       5;
  // constexpr uint8_t SCL_THERM_PIN =     19;
  // constexpr uint8_t SDA_THERM_PIN =     18;

  // // GPS pins
  // constexpr uint8_t GPS_TX_PIN =        35;
  // constexpr uint8_t GPS_RX_PIN =        34;

  // // digital pins
  // constexpr uint8_t DIG1_PIN =          29;
  // constexpr uint8_t DIG2_PIN =          30;
  // constexpr uint8_t DIG3_PIN =          31;
  // constexpr uint8_t DIG4_PIN =          32;

  // // TELEM pins
  // constexpr uint8_t TELEM_RX_PIN =      7;
  // constexpr uint8_t TELEM_TX_PIN =      8;

  // ADC SPI pins
  constexpr uint8_t ADC_SCLK_PIN        13;
  constexpr uint8_t ADC_SDI_PIN         11; // MOSI
  constexpr uint8_t ADC_SDO_PIN =       12; // MISO
  constexpr uint8_t CS_PIN =            10;
  constexpr uint8_t RST_PIN =           1;

  // serial writing
  serial.begin(115200);

}

void loop() {
  // put your main code here, to run repeatedly:

  // collect adc data from pinouts

  // process thermistor data

  // process can data from pinouts
  // CAN PARSER
  can.parse_message();
  
  // test printing can parser output

  Serial.print("Highest selected temp (Temperatures #1): ");
  Serial.print(can.highest);
  Serial.println(" C");

  Serial.print("Motor Temperature: ");
  Serial.print(can.motorTemp);
  Serial.println(" C");
  
  //GPS
  // process gps data
  while(Serial8.available()) {
    char c = Serial8.read();
    if(gps.encode(c)) {
      long lat, lon;
      unsigned long age; //outputs in ms
      int year;
      byte month, day;
      byte hour, minute, second, hundredths;
      float speedMPH = gps.f_speed_mph();


      gps.get_position(&lat, &lon);
      gps.crack_datetime(
        &year,
        &month,
        &day,
        &hour,
        &minute,
        &second,
        &hundredths,
        &age
      );


      if (age != TinyGPS::GPS_INVALID_AGE) {
        Serial.print("Lat: ");
        Serial.println(lat / 1000000.0, 6);

        Serial.print("Lon: ");
        Serial.println(lon / 1000000.0, 6);

        Serial.print("Speed (MPH): ");
        Serial.println(speedMPH);

        printPacificTime(hour, minute, second);
      }
    }
  }

  // telemetry process
  telemSend(elapsed, ambientC, inletC, inletOk, outletC, outletOk, hz, lpm);

  // write to sd card

}
