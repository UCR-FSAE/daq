#include "HardwareSerial.h"
#include "pins_arduino.h"
#include "telem.h"

void telemInit() {
  // for debugging
  //pinMode(LED_BUILTIN, OUTPUT); digitalWrite(LED_BUILTIN, LOW);

  TELEM_SERIAL.begin(TELEM_BAUD, SERIAL_8N1);
}

void telemSend(const CanParser& can, elapsedMillis time, u_int32_t& last_time) {
  if (time - last_time >= 100) {
    TELEM_SERIAL.print("highestTemp=");  TELEM_SERIAL.print(can.highest, 1);
    TELEM_SERIAL.print(",motorTemp=");   TELEM_SERIAL.print(can.motorTemp, 1);
    TELEM_SERIAL.print(",dcI=");         TELEM_SERIAL.print(can.dcBusCurrent, 2);
    TELEM_SERIAL.print(",dcV=");         TELEM_SERIAL.print(can.dcBusVoltage, 2);
    TELEM_SERIAL.print(",outV=");        TELEM_SERIAL.print(can.outputVoltage, 2);
    TELEM_SERIAL.print(",sys12V=");      TELEM_SERIAL.print(can.system12V, 2);
    TELEM_SERIAL.print(",invLock=");     TELEM_SERIAL.print(can.inverterEnableLockout ? 1 : 0);
    TELEM_SERIAL.print(",pFaultLo=");    TELEM_SERIAL.print(can.postFaultLo);
    TELEM_SERIAL.print(",pFaultHi=");    TELEM_SERIAL.print(can.postFaultHi);
    TELEM_SERIAL.print(",rFaultLo=");    TELEM_SERIAL.print(can.runFaultLo);
    TELEM_SERIAL.print(",rFaultHi=");    TELEM_SERIAL.println(can.runFaultHi);
    last_time = time;
  }

  // TO ADD
  // car speed (from wheel speed & IMU)
  // inverter temp?
  // rms voltage & current, pack voltage, power, and SoC (from BMS, thru CAN)
}

void telemSendString(const char* s) {
  TELEM_SERIAL.println(s);
}
