
#include "WheelSpeed.h"

namespace WheelSpeed {

namespace {

// ---------------- CONFIGURATION ----------------

constexpr uint8_t PINS[COUNT] = {
    29, 30, 31, 32
};

// Adjust for actual gear tooth counts
constexpr float PULSES_PER_REV[COUNT] = {
    24.0f, 24.0f, 24.0f, 24.0f
};

// Measurement interval: 20 ms | 50 Hz
constexpr uint32_t UPDATE_US = 20000;

// Switch to multi-pulse averaging at 4 pulses
constexpr uint32_t MULTI_PULSE_THRESHOLD = 4;

// Stop detection
constexpr uint32_t MIN_TIMEOUT_US = 40000;
constexpr uint32_t MAX_TIMEOUT_US = 1000000;

// ---------------- ISR DATA ----------------

volatile uint32_t pulseCount[COUNT] = {};
volatile uint32_t firstPulseUs[COUNT] = {};
volatile uint32_t lastPulseUs[COUNT] = {};
volatile uint32_t lastPeriodUs[COUNT] = {};
volatile bool seenPulse[COUNT] = {};

// ---------------- MODULE DATA ----------------

Measurement data = {};
uint32_t lastUpdateUs = 0;

// ---------------- INTERRUPTS ----------------

void handlePulse(uint8_t wheel) {
    uint32_t now = micros();

    if (seenPulse[wheel]) {
        lastPeriodUs[wheel] =
            now - lastPulseUs[wheel];
    } else {
        seenPulse[wheel] = true;
    }

    lastPulseUs[wheel] = now;

    if (pulseCount[wheel] == 0) {
        firstPulseUs[wheel] = now;
    }

    pulseCount[wheel]++;
}

void isrFL() { handlePulse(FL); }
void isrFR() { handlePulse(FR); }
void isrRL() { handlePulse(RL); }
void isrRR() { handlePulse(RR); }

} // Anonymous namespace

// ---------------- INITIALIZATION ----------------

void begin() {
    for (uint8_t i = 0; i < COUNT; i++) {
        pinMode(PINS[i], INPUT);
    }

    attachInterrupt(
        digitalPinToInterrupt(PINS[FL]),
        isrFL, RISING
    );

    attachInterrupt(
        digitalPinToInterrupt(PINS[FR]),
        isrFR, RISING
    );

    attachInterrupt(
        digitalPinToInterrupt(PINS[RL]),
        isrRL, RISING
    );

    attachInterrupt(
        digitalPinToInterrupt(PINS[RR]),
        isrRR, RISING
    );

    lastUpdateUs = micros();
}

// ---------------- FREQUENCY CALCULATION ----------------

bool update() {
    uint32_t now = micros();

    if ((uint32_t)(now - lastUpdateUs) < UPDATE_US) {
        return false;
    }

    uint32_t counts[COUNT];
    uint32_t first[COUNT];
    uint32_t last[COUNT];
    uint32_t period[COUNT];

    // Copy ISR data atomically
    noInterrupts();

    uint32_t sampleUs = micros();

    for (uint8_t i = 0; i < COUNT; i++) {
        counts[i] = pulseCount[i];
        first[i] = firstPulseUs[i];
        last[i] = lastPulseUs[i];
        period[i] = lastPeriodUs[i];

        pulseCount[i] = 0;
    }

    interrupts();

    lastUpdateUs = sampleUs;

    // Process each wheel
    for (uint8_t i = 0; i < COUNT; i++) {
        float hz = 0.0f;

        if (period[i] > 0) {
            uint32_t age = sampleUs - last[i];

            uint64_t timeout =
                (uint64_t)period[i] * 3;

            if (timeout < MIN_TIMEOUT_US)
                timeout = MIN_TIMEOUT_US;

            if (timeout > MAX_TIMEOUT_US)
                timeout = MAX_TIMEOUT_US;

            if (age <= timeout) {

                // Multiple pulse average
                if (counts[i] >= MULTI_PULSE_THRESHOLD) {
                    uint32_t span = last[i] - first[i];

                    if (span > 0) {
                        hz = (counts[i] - 1) *
                             1000000.0f / span;
                    }
                }

                // Single period measurement
                else {
                    hz = 1000000.0f / period[i];
                }
            }
        }

        data.frequencyHz[i] = hz;

        data.rpm[i] =
            hz * 60.0f / PULSES_PER_REV[i];
    }

    return true;
}

// ---------------- DATA ACCESS ----------------

const Measurement& getData() {
    return data;
}

} // namespace WheelSpeed
