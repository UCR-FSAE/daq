#include "telem.h"

void telemInit() {
  TELEM_SERIAL.begin(TELEM_BAUD, SERIAL_8N1);
}

void telemSend(unsigned long elapsed,
               double ambientC,
               double inletC,   bool inletOk,
               double outletC,  bool outletOk,
               float  hz,       float lpm)
{
  TELEM_SERIAL.print("T=");    TELEM_SERIAL.print(elapsed);
  TELEM_SERIAL.print(",AMB="); TELEM_SERIAL.print(ambientC, 1);
  TELEM_SERIAL.print(",IN=");  TELEM_SERIAL.print(inletOk  ? inletC  : -999.0, 1);
  TELEM_SERIAL.print(",OUT="); TELEM_SERIAL.print(outletOk ? outletC : -999.0, 1);
  TELEM_SERIAL.print(",HZ=");  TELEM_SERIAL.print(hz, 2);
  TELEM_SERIAL.print(",LPM="); TELEM_SERIAL.println(lpm, 2);

  // USB mirror for debugging
  Serial.print("T=");    Serial.print(elapsed);
  Serial.print(",AMB="); Serial.print(ambientC, 1);
  Serial.print(",IN=");  Serial.print(inletOk  ? inletC  : -999.0, 1);
  Serial.print(",OUT="); Serial.print(outletOk ? outletC : -999.0, 1);
  Serial.print(",HZ=");  Serial.print(hz, 2);
  Serial.print(",LPM="); Serial.println(lpm, 2);
}
