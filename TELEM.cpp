#include "HardwareSerial.h"
#include "pins_arduino.h"
#include "telem.h"

void telemInit() {
  // for debugging
  pinMode(LED_BUILTIN, OUTPUT); digitalWrite(LED_BUILTIN, LOW);

  TELEM_SERIAL.begin(TELEM_BAUD);
}

// void telemSend(float highest, float motorTemp, int16_t motorSpeed,
//                float aCurrent, float bCurrent, float cCurrent,
//                float dcCurrent, float dcVoltage, float outVoltage,
//                float system12V, uint8_t invState, bool invLockEnabled,
//                uint16_t pFaultLo, uint16_t pFaultHi, uint16_t rFaultLo, uint16_t rFaultHi,
//                float commTorq, float torqFeedbck, uint32_t pwrOnCount,
//                float pwrOnSec)
// {
//   TELEM_SERIAL.print("highestTemp=");    TELEM_SERIAL.print(highest, 1);
//   TELEM_SERIAL.print(",motorTemp=");     TELEM_SERIAL.print(motorTemp, 1);
//   TELEM_SERIAL.print(",motorSpeed=");    TELEM_SERIAL.print(motorSpeed);
//   TELEM_SERIAL.print(",iA=");           TELEM_SERIAL.print(aCurrent, 2);
//   TELEM_SERIAL.print(",iB=");           TELEM_SERIAL.print(bCurrent, 2);
//   TELEM_SERIAL.print(",iC=");           TELEM_SERIAL.print(cCurrent, 2);
//   TELEM_SERIAL.print(",dcI=");          TELEM_SERIAL.print(dcCurrent, 2);
//   TELEM_SERIAL.print(",dcV=");          TELEM_SERIAL.print(dcVoltage, 2);
//   TELEM_SERIAL.print(",outV=");         TELEM_SERIAL.print(outVoltage, 2);
//   TELEM_SERIAL.print(",sys12V=");       TELEM_SERIAL.print(system12V, 2);
//   TELEM_SERIAL.print(",invState=");     TELEM_SERIAL.print(invState);
//   TELEM_SERIAL.print(",invLock=");      TELEM_SERIAL.print(invLockEnabled ? 1 : 0);
//   TELEM_SERIAL.print(",pFaultLo=");     TELEM_SERIAL.print(pFaultLo);
//   TELEM_SERIAL.print(",pFaultHi=");     TELEM_SERIAL.print(pFaultHi);
//   TELEM_SERIAL.print(",rFaultLo=");     TELEM_SERIAL.print(rFaultLo);
//   TELEM_SERIAL.print(",rFaultHi=");     TELEM_SERIAL.print(rFaultHi);
//   TELEM_SERIAL.print(",cmdTorq=");      TELEM_SERIAL.print(commTorq, 2);
//   TELEM_SERIAL.print(",torqFb=");       TELEM_SERIAL.print(torqFeedbck, 2);
//   TELEM_SERIAL.print(",pwrOnCnt=");     TELEM_SERIAL.print(pwrOnCount);
//   TELEM_SERIAL.print(",pwrOnSec=");     TELEM_SERIAL.println(pwrOnSec, 1);
// }

void telemSend() {
  TELEM_SERIAL.println("hi");
}
