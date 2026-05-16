#include <Arduino.h>

#include <SD.h>
#include <string.h>
#include <stdio.h>

// telemetry process

// gps process

// can process

// sd card process

/*
adjust LOG_PERIOD_MS to the desired write speed in ms
change MAX_LOG_FILES to set the amt of log files before the logger starts overwriting files
note there is enough char space for up to 999 unique log files
the logger will loop back to 001 and overwrite regardless of this limit if it fails to write a packet; i.e. the SD card is full
logidx.bin file stores the index of the current log file being accessed; strange behavior may occur after power off
due to flush/write timing. Make sure to wait at least one second after ending a power cycle before shutting off to insure the 
index is properly saved. This only really matters if you intend on using the same SD card for the next power cycle, as the logger 
may overwrite the previous lap due to the log index not being properly saved. 
*/

const uint32_t LOG_PERIOD_MS = 20;
uint32_t lastLogTime = 0;

struct logPacket {

  uint32_t time_ms;

  float temp_1; // peak value
  float temp_3; // motor temp

  int16_t motor_speed; // rpm

  float phaseACurrent;
  float phaseBCurrent; // optional
  float phaseCCurrent; // optional
  float dcBusCurrent;

  float dcBusVoltage;
  float outputVoltage;
  
  // internal voltage
  float system12V; 

  // internal states
  uint8_t inverterState; 
  bool inverterEnableLockout;

  // Fault Codes: 0x0AB
  uint16_t postFaultLo;
  uint16_t postFaultHi;
  uint16_t runFaultLo;
  uint16_t runFaultHi;

  // Torque & Timer Information: 0x0AC
  float commandedTorque;
  float torqueFeedback;
  uint32_t powerOnTimerCounts;
  float powerOnTimerSeconds;

  // gps
  long lat;
  long lon;


};

class packetLogger {
private:
  File _file;
  logPacket _packet;
  uint16_t _packetsSinceFlush = 0;

  static constexpr uint16_t MAX_LOG_FILES = 999;
  static constexpr const char* INDEX_FILENAME = "logidx.bin";

  uint16_t _nextLogIndex = 1;
  char _currentFilename[13];

  void clearPacket() {
    memset(&_packet, 0, sizeof(_packet));
  }

  void makeFilename(uint16_t index, char* outName, size_t len) {
    snprintf(outName, len, "log%03u.bin", index);
  }

  uint16_t findFirstUnusedIndex() {
    char filename[13];

    for (uint16_t i = 1; i <= MAX_LOG_FILES; i++) {
      makeFilename(i, filename, sizeof(filename));

      if (!SD.exists(filename)) {
        return i;
      }
    }

    // all slots used, start rotating from 1
    return 1;
  }

  bool loadNextLogIndex() {
    File idxFile = SD.open(INDEX_FILENAME, FILE_READ);

    if (idxFile) {
      if (idxFile.available() >= (int)sizeof(_nextLogIndex)) {
        idxFile.read((uint8_t*)&_nextLogIndex, sizeof(_nextLogIndex));
      }
      idxFile.close();

      if (_nextLogIndex < 1 || _nextLogIndex > MAX_LOG_FILES) {
        _nextLogIndex = 1;
      }

      return true;
    }

    // If no index file exists yet, choose first unused slot
    _nextLogIndex = findFirstUnusedIndex();
    return true;
  }

  bool saveNextLogIndex() {
    SD.remove(INDEX_FILENAME);

    File idxFile = SD.open(INDEX_FILENAME, FILE_WRITE);
    if (!idxFile) {
      Serial.println("Failed to save log index file.");
      return false;
    }

    size_t written = idxFile.write((const uint8_t*)&_nextLogIndex, sizeof(_nextLogIndex));
    idxFile.close();

    return (written == sizeof(_nextLogIndex));
  }

  bool openLogSlot(uint16_t index) {
    makeFilename(index, _currentFilename, sizeof(_currentFilename));

    // overwrite the slot if it already exists
    if (SD.exists(_currentFilename)) {
      SD.remove(_currentFilename);
    }

    _file = SD.open(_currentFilename, FILE_WRITE);

    if (!_file) {
      Serial.print("Failed to open ");
      Serial.println(_currentFilename);
      return false;
    }

    Serial.print("Opened log file: ");
    Serial.println(_currentFilename);

    _packetsSinceFlush = 0;
    return true;
  }

  bool openNextRotatingFile() {
    if (!loadNextLogIndex()) {
      return false;
    }

    uint16_t currentIndex = _nextLogIndex;

    if (!openLogSlot(currentIndex)) {
      return false;
    }

    // advance pointer for next time
    _nextLogIndex++;
    if (_nextLogIndex > MAX_LOG_FILES) {
      _nextLogIndex = 1;
    }

    saveNextLogIndex();
    return true;
  }

  bool rotateToNextFile() {
    if (_file) {
      _file.flush();
      _file.close();
    }

    Serial.println("Rotating to next log file...");
    return openNextRotatingFile();
  }

public:
  bool begin() {
    clearPacket();

    if (!SD.begin(BUILTIN_SDCARD)) {
      Serial.println("SD card failed to initialize.");
      return false;
    }

    Serial.println("SD card initialized.");

    return openNextRotatingFile();
  }

  bool write() {
    if (!_file) {
      Serial.println("No open log file.");
      return false;
    }

    _packet.time_ms = millis();

    size_t written = _file.write((const uint8_t*)&_packet, sizeof(_packet));

    if (written != sizeof(_packet)) {
      Serial.println("Write failed. Card may be full. Trying next log slot...");

      if (!rotateToNextFile()) {
        Serial.println("Failed to rotate to next log file.");
        return false;
      }

      // retry once in the new file
      written = _file.write((const uint8_t*)&_packet, sizeof(_packet));

      if (written != sizeof(_packet)) {
        Serial.println("Retry write failed.");
        return false;
      }
    }

    _packetsSinceFlush++;

    if (_packetsSinceFlush >= 50) {
      _file.flush();
      _packetsSinceFlush = 0;
    }

    return true;
  }

  void flush() {
    if (_file) {
      _file.flush();
      _packetsSinceFlush = 0;
    }
  }

  void close() {
    if (_file) {
      _file.flush();
      _file.close();
    }
  }

  // ---- setters ----
  void setTemps(float temp1, float temp3) {
    _packet.temp_1 = temp1;
    _packet.temp_3 = temp3;
  }

  void setMotorSpeed(int16_t rpm) {
    _packet.motor_speed = rpm;
  }

  void setCurrents(float phaseA, float phaseB, float phaseC, float dcBus) {
    _packet.phaseACurrent = phaseA;
    _packet.phaseBCurrent = phaseB;
    _packet.phaseCCurrent = phaseC;
    _packet.dcBusCurrent = dcBus;
  }

  void setVoltages(float dcBusV, float outputV, float system12V) {
    _packet.dcBusVoltage = dcBusV;
    _packet.outputVoltage = outputV;
    _packet.system12V = system12V;
  }

  void setInverterState(uint8_t state, bool lockout) {
    _packet.inverterState = state;
    _packet.inverterEnableLockout = lockout;
  }

  void setFaultCodes(uint16_t postLo, uint16_t postHi, uint16_t runLo, uint16_t runHi) {
    _packet.postFaultLo = postLo;
    _packet.postFaultHi = postHi;
    _packet.runFaultLo = runLo;
    _packet.runFaultHi = runHi;
  }

  void setTorque(float commanded, float feedback) {
    _packet.commandedTorque = commanded;
    _packet.torqueFeedback = feedback;
  }

  void setPowerOnTimer(uint32_t counts, float seconds) {
    _packet.powerOnTimerCounts = counts;
    _packet.powerOnTimerSeconds = seconds;
  }

  void setGPS(int32_t lat, int32_t lon) {
    _packet.lat = lat;
    _packet.lon = lon;
  }
};

packetLogger logger;
bool loggerReady = false;
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
  constexpr uint8_t ADC_SCLK_PIN =      13;
  constexpr uint8_t ADC_SDI_PIN =       11; // MOSI
  constexpr uint8_t ADC_SDO_PIN =       12; // MISO
  constexpr uint8_t CS_PIN =            10;
  constexpr uint8_t RST_PIN =           1;

  // serial writing
  Serial.begin(115200);

  // SD Card init
  loggerReady = logger.begin();
  if (!loggerReady) {
    Serial.println("Logger failed to start.");
  }
}

void loop() {
  // put your main code here, to run repeatedly:

  // collect adc data from pinouts

  // process thermistor data

  // process can data from pinouts
  
  // process gps data

  // telemetry process

  // write to sd card
  uint32_t now = millis();

  if ( loggerReady && now - lastLogTime >= LOG_PERIOD_MS) {
    
    lastLogTime = now;
    /* check the log packet struct for data types & their positions*/
    logger.setTemps(/*inverterTemp*/0, /*motorTemp*/0);
    logger.setMotorSpeed(/*rpm*/0);
    logger.setVoltages(/*dcVoltage*/0, 0.0, 12.0);
    logger.setCurrents(0.0, 0.0, 0.0, /*dcCurrent*/0);
    logger.setGPS(/*gpsLat*/0, /*gpsLon*/0  );

    if (!logger.write()) {
      Serial.println("Logger write failed.");
      loggerReady = false;
    }
  }
}
