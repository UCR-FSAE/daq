
#include <Arduino.h>
#include "WheelSpeed.h"

void setup() {
    Serial.begin(115200);

    WheelSpeed::begin();
}

void loop() {
    if (WheelSpeed::update()) {

        const auto& ws = WheelSpeed::getData();

        // refer to WheelSpeed.h for RPM/Hz data calls. Example read shown below:
        // do NOT use Serial for any timing operations, as it can potentially 
        //introduce delays and affect the accuracy of wheel speed measurements.
        Serial.printf(
            "FL: %.1f | FR: %.1f | RL: %.1f | RR: %.1f RPM\n",
            ws.rpm[WheelSpeed::FL],
            ws.rpm[WheelSpeed::FR],
            ws.rpm[WheelSpeed::RL],
            ws.rpm[WheelSpeed::RR]
        );
    }
}
