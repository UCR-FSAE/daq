#include "HardwareSerial.h"
#include "pins_arduino.h"
#include "telem.h"

void telemInit() {
  // for debugging
  pinMode(LED_BUILTIN, OUTPUT); digitalWrite(LED_BUILTIN, LOW);

  TELEM_SERIAL.begin(TELEM_BAUD, SERIAL_8N1);
}

void telemSend(const CanParser& can) {
  TELEM_SERIAL.print("highestTemp=");  TELEM_SERIAL.print(can.highest, 1);
  TELEM_SERIAL.print(",motorTemp=");   TELEM_SERIAL.print(can.motorTemp, 1);
  TELEM_SERIAL.print(",motorSpeed=");  TELEM_SERIAL.print(can.motorSpeed);
  TELEM_SERIAL.print(",iA=");          TELEM_SERIAL.print(can.phaseACurrent, 2);
  TELEM_SERIAL.print(",iB=");          TELEM_SERIAL.print(can.phaseBCurrent, 2);
  TELEM_SERIAL.print(",iC=");          TELEM_SERIAL.print(can.phaseCCurrent, 2);
  TELEM_SERIAL.print(",dcI=");         TELEM_SERIAL.print(can.dcBusCurrent, 2);
  TELEM_SERIAL.print(",dcV=");         TELEM_SERIAL.print(can.dcBusVoltage, 2);
  TELEM_SERIAL.print(",outV=");        TELEM_SERIAL.print(can.outputVoltage, 2);
  TELEM_SERIAL.print(",sys12V=");      TELEM_SERIAL.print(can.system12V, 2);
  TELEM_SERIAL.print(",invState=");    TELEM_SERIAL.print(can.inverterState);
  TELEM_SERIAL.print(",invLock=");     TELEM_SERIAL.print(can.inverterEnableLockout ? 1 : 0);
  TELEM_SERIAL.print(",pFaultLo=");    TELEM_SERIAL.print(can.postFaultLo);
  TELEM_SERIAL.print(",pFaultHi=");    TELEM_SERIAL.print(can.postFaultHi);
  TELEM_SERIAL.print(",rFaultLo=");    TELEM_SERIAL.print(can.runFaultLo);
  TELEM_SERIAL.print(",rFaultHi=");    TELEM_SERIAL.print(can.runFaultHi);
  TELEM_SERIAL.print(",cmdTorq=");     TELEM_SERIAL.print(can.commandedTorque, 2);
  TELEM_SERIAL.print(",torqFb=");      TELEM_SERIAL.print(can.torqueFeedback, 2);
  TELEM_SERIAL.print(",pwrOnCnt=");    TELEM_SERIAL.print(can.powerOnTimerCounts);
  TELEM_SERIAL.print(",pwrOnSec=");    TELEM_SERIAL.println(can.powerOnTimerSeconds, 1);
}