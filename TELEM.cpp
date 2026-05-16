#include "telem.h"

void printField(const char* label, float value, int decimals) {
  TELEM_SERIAL.print(label);
  TELEM_SERIAL.print('=');
  TELEM_SERIAL.print(value, decimals);
  TELEM_SERIAL.print(',');
}

void printFieldInt(const char* label, int32_t value) {
  TELEM_SERIAL.print(label);
  TELEM_SERIAL.print('=');
  TELEM_SERIAL.print(value);
  TELEM_SERIAL.print(',');
}

void telemInit() {
  TELEM_SERIAL.begin(TELEM_BAUD, SERIAL_8N1);
}

void telemSend(const logPacket& pkt) {
  printField("highestTemp", pkt.temp_1,              1);
  printField("motorTemp",   pkt.temp_3,              1);
  printFieldInt("motorSpeed",  pkt.motor_speed        );
  printField("iA",          pkt.phaseACurrent,       2);
  printField("iB",          pkt.phaseBCurrent,       2);
  printField("iC",          pkt.phaseCCurrent,       2);
  printField("dcI",         pkt.dcBusCurrent,        2);
  printField("dcV",         pkt.dcBusVoltage,        2);
  printField("outV",        pkt.outputVoltage,       2);
  printField("sys12V",      pkt.system12V,           2);
  printFieldInt("invState",    pkt.inverterState      );
  printFieldInt("invLock",     pkt.inverterEnableLockout);
  printFieldInt("pFaultLo",    pkt.postFaultLo        );
  printFieldInt("pFaultHi",    pkt.postFaultHi        );
  printFieldInt("rFaultLo",    pkt.runFaultLo         );
  printFieldInt("rFaultHi",    pkt.runFaultHi         );
  printField("cmdTorq",     pkt.commandedTorque,     2);
  printField("torqFb",      pkt.torqueFeedback,      2);
  printFieldInt("pwrOnCnt",    pkt.powerOnTimerCounts );
  printField("pwrOnSec",    pkt.powerOnTimerSeconds, 1);

  TELEM_SERIAL.println();
}