#include <Arduino.h>
#include <elapsedMillis.h>
#include "faults.h"
// See "DAQ DOC" for peripheral PNs
// See "VCU Pinouts" google sheet for pinouts

// current_time_ms is always an up-to-date variable storing the elapsed time since startup in milliseconds
// USE CURRENT_TIME_MS FOR INTERUPT TIMESTAMPS AND DATA SENDING RATES
elapsedMillis current_time_ms;

// ---- DEMO ONLY: fake handlers so you can see the actions fire ----
// Real ones will live in the telemetry (LoRa) and IMU modules.
static bool demo_imu_alive = true;
void demoPitAlert(Fault f, bool active) {
  Serial.print("  -> PIT ALERT over LoRa: ");
  Serial.print(faultName(f));
  Serial.println(active ? " ON" : " cleared");
}
void demoRecoverImu() {
  Serial.println("  -> trying to re-init IMU...");
  if (current_time_ms > 6500) demo_imu_alive = true;  // pretend the 3rd try works
}
// ---- END DEMO ----

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

  // ---- DEMO ONLY plug in what "alert pit" and "recover" do (DEMO versions for now)
  setPitAlertHandler(demoPitAlert);
  setRecoverHandler(FAULT_IMU_STALE, demoRecoverImu);
  // ---- END DEMO ----
}

void loop() {
  // put your main code here, to run repeatedly:

  // collect adc data (4x shock pots, 1x steering pot, 1x flow meter)

  // collect thermistor data (4x brake pad temps, 2x coolant temps)

  // collect can data (CAN lol, im not writing allat)
  
  // collect gps data (speed, time)

  // collect IMU data (longitudinal, lateral, & vertical accel as well as roll pitch & yaw rate)

  // write to telemetry

  // write to sd card

  // ---- DEMO ONLY (delete once real sensors are wired up) ----
  // Pretend every sensor is sending data, then break things on purpose:
  //   4 s: IMU dies      -> IMU_STALE ON (act), recovery retries every 1 s, fixed ~7 s
  //   8 s: SD write fails -> SD_WRITE_FAIL ON (act) + pit alert, cleared at 9 s
  //  10 s: wheel speed dies -> WHEEL_SPEED_STALE ON (log only)
  uint32_t t = current_time_ms;
  if (t > 4000 && t < 4100) demo_imu_alive = false;
  if (t < 10000) last_wheel_speed_ms = t;
  if (demo_imu_alive) last_imu_ms = t;
  last_adc_ms = last_tc_ms = last_gps_ms = t;
  last_can_inv_ms = last_can_bms_ms = last_can_vcu_ms = t;
  if (t >= 8000 && t < 9000) setFault(FAULT_SD_WRITE_FAIL);
  if (t >= 9000 && hasFault(FAULT_SD_WRITE_FAIL)) clearFault(FAULT_SD_WRITE_FAIL);
  // ---- END DEMO ---- ------ --------       --       -    -

  // check for faults + run their actions (keep this at the end of loop)
  checkFaults(current_time_ms);
}