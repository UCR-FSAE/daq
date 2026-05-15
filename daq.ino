#include <SPI.h>
#include "Adafruit_MAX31855.h"
#include <math.h>
// telemetry process

// gps process

// can process

// sd card process

// therm process: MCP96RL00

// adc process: ADS8688IDBTR
#define TC1_DO   8
#define TC1_CS   7
#define TC1_CLK  6

#define TC2_DO   5
#define TC2_CS   4
#define TC2_CLK  3

#define FLOW_PIN        2
#define LPM_PER_HZ      0.31f
#define SAMPLE_INTERVAL 1000UL

#define COOLANT_WARN_C   90.0
#define COOLANT_CRIT_C  105.0
#define DEBOUNCE_US      500

// ── Objects ──────────────────────────────────────────────────────────────
Adafruit_MAX31855 tcInlet (TC1_CLK, TC1_CS, TC1_DO);
Adafruit_MAX31855 tcOutlet(TC2_CLK, TC2_CS, TC2_DO);

// ── Flow meter ───────────────────────────────────────────────────────────
volatile unsigned long pulseCount    = 0;
volatile unsigned long lastPulseTime = 0;
unsigned long lastSampleTime = 0;
float currentLPM = 0.0;
float currentHz  = 0.0;

// Part of SD state declarations but still necessary for reading adc
unsigned long startMillis     = 0;

// ── ISR ──────────────────────────────────────────────────────────────────
void pulseISR() {
  unsigned long t = micros();
  if (t - lastPulseTime > DEBOUNCE_US) {
    pulseCount++;
    lastPulseTime = t;
  }
}

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

  // All CS HIGH before touching SPI
  pinMode(TC1_CS, OUTPUT); digitalWrite(TC1_CS, HIGH);
  pinMode(TC2_CS, OUTPUT); digitalWrite(TC2_CS, HIGH);
  pinMode(SD_CS,  OUTPUT); digitalWrite(SD_CS,  HIGH);

  delay(100);
  tcInlet.begin();
  delay(50);
  tcOutlet.begin();
  delay(50);

  pinMode(FLOW_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), pulseISR, FALLING);

  startMillis    = millis();
  lastSampleTime = startMillis;
  delay(500);
}

void loop() {
  // put your main code here, to run repeatedly:

  // collect adc data from pinouts
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

  // ── Read sensors ──
  unsigned long elapsed = now - startMillis;
  double ambientC = tcInlet.readInternal();
  double inletC   = tcInlet.readCelsius();
  double outletC  = tcOutlet.readCelsius();
  bool   inletOk  = !isnan(inletC);
  bool   outletOk = !isnan(outletC);
  // process thermistor data

  // process can data from pinouts
  
  // process gps data

  // telemetry process

  // write to sd card
}
