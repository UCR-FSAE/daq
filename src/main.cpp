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
void demoSdLog(FaultRecord type, uint32_t time_ms, const void *data, uint8_t len) {
  (void)time_ms; (void)data;  // the real one passes these to SDLogger::writeRecord
  Serial.print("  -> SD LOG: ");
  Serial.print(type == FREC_DAQ_FAULTS ? "DAQ faults" : "inverter faults");
  Serial.print(", ");
  Serial.print(len);
  Serial.println(" bytes");
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
  setLogHandler(demoSdLog);
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
  // Simulate sensor data, then trigger faults:
  //  4 s: IMU stops -> IMU_STALE ON, retry every 1 s, fixed at ~7 s
  //  5 s: Inverter over-current -> logged via 0x0AB, cleared at 6 s
  //  8 s: SD write fails -> SD_WRITE_FAIL ON + pit alert, cleared at 9 s
  // 10 s: GPS stops -> GPS_STALE ON at ~13 s (3 s timeout, log only), stays on
  // 15 s: VCU goes silent on CAN -> CAN_VCU_STALE ON ~100 ms later + pit alert
  // 17 s: ADC also stops -> ADC_STALE ON; ALL FAULTS shows 3 faults at once
  // 19 s: VCU back -> CAN_VCU_STALE cleared, ADC + GPS stay on
  // 21 s: ADC back -> ADC_STALE cleared, only GPS left
  uint32_t t = current_time_ms;
  if (t > 4000 && t < 4100) demo_imu_alive = false;
  if (demo_imu_alive) last_imu_ms = t;
  if (t < 10000) last_gps_ms = t;
  if (t < 15000 || t >= 19000) last_can_vcu_ms = t;  // VCU silent 15-19 s
  if (t < 17000 || t >= 21000) last_adc_ms = t;      // ADC silent 17-21 s
  last_tc_ms = t;
  last_can_inv_ms = last_can_bms_ms = t;
  if (t >= 8000 && t < 9000) setFault(FAULT_SD_WRITE_FAIL);
  if (t >= 9000 && hasFault(FAULT_SD_WRITE_FAIL)) clearFault(FAULT_SD_WRITE_FAIL);
// Simulate 0x0AB each loop; real code calls this when CanParser receives 0x0AB.
  uint16_t demo_run_lo = (t >= 5000 && t < 6000) ? 0x0002 : 0;  // byte 4 bit 1 = over-current
  logInverterFaults(0, 0, demo_run_lo, 0, t);
  // ---- END DEMO ----

  // check for faults + run their actions (keep this at the end of loop)
  checkFaults(current_time_ms);
}