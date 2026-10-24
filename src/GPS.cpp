namespace {

void printPacificTime(byte hour, byte minute, byte second) {
  int pacificHour = hour - 7; // PDT. Use -8 for PST.

  if (pacificHour < 0) {
    pacificHour += 24;
  }

  String ampm = "AM";

  if (pacificHour >= 12) {
    ampm = "PM";
  }

  int displayHour = pacificHour % 12;

  if (displayHour == 0) {
    displayHour = 12;
  }

  Serial.print("Pacific Time: ");

  if (displayHour < 10) Serial.print("0");
  Serial.print(displayHour);

  Serial.print(":");

  if (minute < 10) Serial.print("0");
  Serial.print(minute);

  Serial.print(":");

  if (second < 10) Serial.print("0");
  Serial.print(second);

  Serial.print(" ");
  Serial.println(ampm);
}

while(Serial8.available()) {
    char c = Serial8.read();
    if(gps.encode(c)) {
      long lat, lon;
      unsigned long age; //outputs in ms
      int year;
      byte month, day;
      byte hour, minute, second, hundredths;
      float speedMPH = gps.f_speed_mph();


      gps.get_position(&lat, &lon);
      gps.crack_datetime(
        &year,
        &month,
        &day,
        &hour,
        &minute,
        &second,
        &hundredths,
        &age
      );


      if (age != TinyGPS::GPS_INVALID_AGE) {
        Serial.print("Lat: ");
        Serial.println(lat / 1000000.0, 6);

        Serial.print("Lon: ");
        Serial.println(lon / 1000000.0, 6);

        Serial.print("Speed (MPH): ");
        Serial.println(speedMPH);

        printPacificTime(hour, minute, second);
      }
    }
}

void gpsInit() {
    GPS_SERIAl.begin(GPS_BAUD);
}

bool update();

} // namespace gps