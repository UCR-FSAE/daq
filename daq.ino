#include <SPI.h>
#include <SD.h>
#include "Adafruit_MAX31855.h"
#include <math.h>
// telemetry process

// gps process

// can process

// sd card process
#define TC1_DO   8
#define TC1_CS   7
#define TC1_CLK  6

#define TC2_DO   5
#define TC2_CS   4
#define TC2_CLK  3

#define SD_CS           10
#define MAX_FILE_ROWS   1000
#define MAX_FILES       99

#define FLOW_PIN        2
#define LPM_PER_HZ      0.31f
#define SAMPLE_INTERVAL 1000UL

#define COOLANT_WARN_C   90.0
#define COOLANT_CRIT_C  105.0
#define DEBOUNCE_US      500

// ── SD CD (card-detect) pin ───────────────────────────────────────────────
// Most SD modules have a CD pin that goes LOW when a card is seated.
// Wire it to SD_CD_PIN and set HAS_CD true.
// If your module has NO card-detect pin, set HAS_CD false —
// the code will still recover by attempting re-init on every failed write.
#define SD_CD_PIN  9
#define HAS_CD     false   // ← set true if you have the CD pin wired

bool plotMode = false;   // false = human-readable serial, easier to debug
bool dummyTest = false;

// ── Objects ──────────────────────────────────────────────────────────────
Adafruit_MAX31855 tcInlet (TC1_CLK, TC1_CS, TC1_DO);
Adafruit_MAX31855 tcOutlet(TC2_CLK, TC2_CS, TC2_DO);

// ── Flow meter ───────────────────────────────────────────────────────────
volatile unsigned long pulseCount    = 0;
volatile unsigned long lastPulseTime = 0;
unsigned long lastSampleTime = 0;
float currentLPM = 0.0;
float currentHz  = 0.0;

// ── SD state ─────────────────────────────────────────────────────────────
bool     sdMounted  = false;   // true = SD.begin() succeeded and card is in
bool     sdPaused   = false;   // true = user manually paused with 's'
char     logFileName[16];
uint16_t rowsInFile = 0;
uint8_t  fileIndex  = 1;

unsigned long startMillis     = 0;
unsigned long lastMountAttempt = 0;
#define MOUNT_RETRY_MS  2000UL  // try to re-init card every 2 s when absent

// ── Free RAM ─────────────────────────────────────────────────────────────
int freeRAM() {
  extern int __heap_start, *__brkval;
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

// ── ISR ──────────────────────────────────────────────────────────────────
void pulseISR() {
  unsigned long t = micros();
  if (t - lastPulseTime > DEBOUNCE_US) {
    pulseCount++;
    lastPulseTime = t;
  }
}

// ── Helpers ──────────────────────────────────────────────────────────────
float toF(float c) { return c * 9.0f / 5.0f + 32.0f; }

const char* statusLabel(float c) {
  if (c >= COOLANT_CRIT_C) return "CRITICAL";
  if (c >= COOLANT_WARN_C) return "WARNING";
  return "OK";
}

void buildFileName(uint8_t index) {
  snprintf(logFileName, sizeof(logFileName), "cool%02d.csv", index);
}

void writeHeader(File &f) {
  f.println(F("time_ms,ambient_C,"
              "inlet_C,inlet_F,inlet_status,"
              "outlet_C,outlet_F,outlet_status,"
              "delta_C,flow_hz,flow_lpm"));
}

// ── Card-detect helper ────────────────────────────────────────────────────
// Returns true if a card appears to be physically present.
bool cardPresent() {
#if HAS_CD
  return (digitalRead(SD_CD_PIN) == LOW);  // CD pin LOW = card seated (typical)
#else
  return true;  // assume present; rely on write failure to detect removal
#endif
}

// ── Mount / re-mount SD ───────────────────────────────────────────────────
// Call this whenever sdMounted is false.
// Returns true if the card is now ready.
bool mountSD() {
  // Release SPI bus cleanly before re-init
  digitalWrite(SD_CS, HIGH);
  delay(10);

  SD.end();   // make sure previous state is cleared (harmless if never inited)
  delay(10);

  if (!SD.begin(SD_CS)) {
    if (!plotMode) Serial.println(F("[SD] begin() failed — card absent or error"));
    sdMounted = false;
    return false;
  }

  sdMounted = true;
  if (!plotMode) Serial.println(F("[SD] Card mounted OK"));

  // Find or create the correct log file
  return openNextFile();
}

// ── Open / resume log file ────────────────────────────────────────────────
bool openNextFile() {
  uint8_t lastFound = 0;
  for (uint8_t i = 1; i <= MAX_FILES; i++) {
    buildFileName(i);
    if (SD.exists(logFileName)) lastFound = i;
    else break;
  }

  if (lastFound == 0) {
    fileIndex = 1;
  } else {
    buildFileName(lastFound);
    File f = SD.open(logFileName, FILE_READ);
    uint16_t lines = 0;
    if (f) {
      while (f.available()) if (f.read() == '\n') lines++;
      f.close();
      digitalWrite(SD_CS, HIGH);
    }
    if (lines > 0) lines--;

    if (lines < MAX_FILE_ROWS) {
      fileIndex  = lastFound;
      rowsInFile = lines;
      if (!plotMode) {
        Serial.print(F("[SD] Resuming "));
        Serial.print(logFileName);
        Serial.print(F("  row "));
        Serial.println(rowsInFile);
      }
      return true;
    } else {
      fileIndex = lastFound + 1;
    }
  }

  buildFileName(fileIndex);
  File f = SD.open(logFileName, FILE_WRITE);
  if (!f) {
    if (!plotMode) Serial.println(F("[SD] ERROR: could not create file"));
    sdMounted = false;
    return false;
  }
  writeHeader(f);
  f.close();
  digitalWrite(SD_CS, HIGH);
  rowsInFile = 0;

  if (!plotMode) {
    Serial.print(F("[SD] Logging to "));
    Serial.println(logFileName);
  }
  return true;
}

// ── Write one data row ────────────────────────────────────────────────────
// Returns true on success, false if the card is gone.
bool writeRow(unsigned long elapsed,
              double ambC,
              double inC,  bool inOk,
              double outC, bool outOk) {

  // Roll file if full
  if (rowsInFile >= MAX_FILE_ROWS) {
    fileIndex++;
    buildFileName(fileIndex);
    File nf = SD.open(logFileName, FILE_WRITE);
    if (!nf) { sdMounted = false; return false; }
    writeHeader(nf);
    nf.close();
    digitalWrite(SD_CS, HIGH);
    rowsInFile = 0;
    if (!plotMode) {
      Serial.print(F("[SD] Rolled to "));
      Serial.println(logFileName);
    }
  }

  File f = SD.open(logFileName, FILE_WRITE);
  if (!f) {
    // Write failed — card was likely removed
    sdMounted = false;
    if (!plotMode) Serial.println(F("[SD] Write failed — card removed?"));
    return false;
  }

  f.print(elapsed);     f.print(',');
  f.print(ambC, 1);     f.print(',');

  if (inOk) {
    f.print(inC, 1);         f.print(',');
    f.print(toF(inC), 1);    f.print(',');
    f.print(statusLabel(inC));
  } else {
    f.print(F("FAULT,FAULT,FAULT"));
  }
  f.print(',');

  if (outOk) {
    f.print(outC, 1);        f.print(',');
    f.print(toF(outC), 1);   f.print(',');
    f.print(statusLabel(outC));
  } else {
    f.print(F("FAULT,FAULT,FAULT"));
  }
  f.print(',');

  if (inOk && outOk) f.print(inC - outC, 1);
  else               f.print(F("N/A"));
  f.print(',');

  f.print(currentHz,  2); f.print(',');
  f.print(currentLPM, 3);
  f.println();

  f.close();                  // ← flush to card on every row
  digitalWrite(SD_CS, HIGH);  // release SPI bus
  rowsInFile++;
  return true;
}
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
  Serial.begin(9600);
  while (!Serial) delay(1);

  if (!plotMode) Serial.println(F("=== Coolant Logger (hot-swap SD) ==="));

  // All CS HIGH before touching SPI
  pinMode(TC1_CS, OUTPUT); digitalWrite(TC1_CS, HIGH);
  pinMode(TC2_CS, OUTPUT); digitalWrite(TC2_CS, HIGH);
  pinMode(SD_CS,  OUTPUT); digitalWrite(SD_CS,  HIGH);

#if HAS_CD
  pinMode(SD_CD_PIN, INPUT_PULLUP);
#endif

  delay(100);
  tcInlet.begin();
  delay(50);
  tcOutlet.begin();
  delay(50);

  pinMode(FLOW_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), pulseISR, FALLING);

  if (!plotMode) Serial.println(F("TCs + flow meter OK"));

  mountSD();   // attempt initial mount — OK if it fails, loop will retry

  if (!plotMode) {
    Serial.print(F("Free RAM: "));
    Serial.print(freeRAM());
    Serial.println(F(" bytes"));
    Serial.println(F("Send 's' to pause/resume logging."));
    Serial.println(F("Pull card any time — re-insert to resume."));
  }

  startMillis    = millis();
  lastSampleTime = startMillis;
  delay(500);
}

void loop() {
  // put your main code here, to run repeatedly:

  // collect adc data from pinouts
  // ── Serial command ──
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 's' || c == 'S') {
      sdPaused = !sdPaused;
      if (!plotMode) {
        Serial.print(F("Logging "));
        Serial.println(sdPaused ? F("PAUSED by user") : F("RESUMED by user"));
      }
    }
  }

  unsigned long now = millis();

  // ── Flow ──
  if (now - lastSampleTime >= SAMPLE_INTERVAL) {
    noInterrupts();
    unsigned long count = pulseCount;
    pulseCount = 0;
    interrupts();
    float dt   = (now - lastSampleTime) / 1000.0f;
    currentHz  = count / dt;
    currentLPM = currentHz * LPM_PER_HZ;
    lastSampleTime = now;
  }

  // ── Release SD CS before TC reads ──
  digitalWrite(SD_CS, HIGH);

  // ── Read sensors ──
  unsigned long elapsed = now - startMillis;
  double ambientC = tcInlet.readInternal();
  double inletC   = tcInlet.readCelsius();
  double outletC  = tcOutlet.readCelsius();
  bool   inletOk  = !isnan(inletC);
  bool   outletOk = !isnan(outletC);

  // ── Serial output ──
  if (plotMode) {
    double pIn  = inletOk  ? inletC  : 0;
    double pOut = outletOk ? outletC : 0;
    Serial.print("InletC:");   Serial.print(pIn, 1);
    Serial.print(",OutletC:"); Serial.print(pOut, 1);
    Serial.print(",DeltaC:");  Serial.print(inletOk && outletOk ? inletC-outletC : 0, 1);
    Serial.print(",FlowLPM:"); Serial.println(currentLPM, 3);
  } else {
    Serial.print(F("[T+"));
    Serial.print(elapsed / 1000);
    Serial.print(F("s] in="));
    Serial.print(inletOk  ? inletC  : -999, 1);
    Serial.print(F(" out="));
    Serial.print(outletOk ? outletC : -999, 1);
    Serial.print(F(" dT="));
    Serial.print((inletOk && outletOk) ? inletC-outletC : 0, 1);
    Serial.print(F(" LPM="));
    Serial.print(currentLPM, 2);

    // SD status indicator on every line so you can see it live
    if (sdPaused) {
      Serial.print(F("  [SD PAUSED]"));
    } else if (!sdMounted) {
      Serial.print(F("  [SD ABSENT]"));
    } else {
      Serial.print(F("  ["));
      Serial.print(logFileName);
      Serial.print(F(" row "));
      Serial.print(rowsInFile);
      Serial.print(F("]"));
    }
    Serial.println();
  }
  // process thermistor data

  // process can data from pinouts
  
  // process gps data

  // telemetry process

  // write to sd card
  // ── SD: attempt remount if card absent ──────────────────────────────────
  // This is the key section for hot-swap:
  //   - If no card-detect pin: try re-init every MOUNT_RETRY_MS
  //   - If card-detect pin wired: only try when CD says card is present
  if (!sdMounted && !sdPaused) {
    if (now - lastMountAttempt >= MOUNT_RETRY_MS) {
      lastMountAttempt = now;
      if (cardPresent()) {
        if (!plotMode) Serial.println(F("[SD] Card detected — attempting mount..."));
        mountSD();
      }
    }
  }

  // ── SD: write row ────────────────────────────────────────────────────────
  if (sdMounted && !sdPaused) {
    bool ok = writeRow(elapsed, ambientC,
                       inletC,  inletOk,
                       outletC, outletOk);
    if (!ok) {
      // writeRow already set sdMounted = false
      // The retry loop above will pick it up next cycle
      if (!plotMode) Serial.println(F("[SD] Will retry mount in 2s..."));
    }
  }

  delay(1000);
}
