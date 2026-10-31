#include <GPS.h>

TinyGPS NEO_M8N;
unsigned long start_time_utc = 0;  // want to sync the time since starting in ms on daq & gps
unsigned long start_time_ms = 0;
gps::Measurement data = {};

void gps::gpsInit(elapsedMillis* daq_time) {
  // figure out how to only recieve speed and time from the NEO-M8N chip

  // Adding size to the gps serial buffer in case other code (ie sd card) stalls
  static uint8_t gpsExtra[2048];
  GPS_SERIAL.addMemoryForRead(gpsExtra, sizeof(gpsExtra));

  GPS_SERIAL.begin(GPS_BAUD);

  // safety read to initialize gps timestamps
  while(GPS_SERIAL.available() && !NEO_M8N.encode(GPS_SERIAL.read())) {}
  start_time_ms = *daq_time;
  NEO_M8N.get_datetime(NULL, &start_time_utc, NULL);
}

bool gps::update() {
  bool updated = false;

  while(GPS_SERIAL.available()) {  // technically non-blocking because buffer is fixed at 2048, need to make sure this doesn't cause issues
    if (NEO_M8N.encode(GPS_SERIAL.read())){
      updated = true;
    }
  }

  if (!updated) return false;

  unsigned long gps_time_utc; // returns a single value hhmmsscc, so 12:34:56.78 is 12345678

  NEO_M8N.get_datetime(NULL, &gps_time_utc, &data.age);
  data.daq_ms = (utcToMs(gps_time_utc) - utcToMs(start_time_utc)) + start_time_ms - GPS_DELAY;
  data.speed = NEO_M8N.f_speed_mph();

  return true;
}

gps::Measurement& gps::getData(){
  return data;
}

// helper, converts hhmmsscs (utc) time into miliseconds elapsed since midnight
uint32_t utcToMs(uint32_t utc_time) {
  uint8_t hh = utc_time / 1000000;
  uint8_t mm = (utc_time / 10000) % 100;
  uint8_t ss = (utc_time / 100) % 100;
  uint8_t cs = utc_time % 100;

  return ((hh * 60UL + mm) * 60UL + ss) * 1000UL + cs * 10UL;
}