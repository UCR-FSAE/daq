
#pragma once

#include <Arduino.h>

namespace WheelSpeed {

enum Wheel : uint8_t {
    FL = 0,
    FR,
    RL,
    RR,
    COUNT
};

struct Measurement {
    float frequencyHz[COUNT];
    float rpm[COUNT];
};

// Initialize GPIO and interrupts
void begin();

// Returns true when new readings are available
bool update();

// Access latest measurements
const Measurement& getData();

}
