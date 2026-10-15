#include <Arduino.h>

// Wheel indexing
enum Wheel : uint8_t {
    FL = 0,
    FR = 1,
    RL = 2,
    RR = 3
};

constexpr uint8_t NUM_WHEELS = 4;

// GPIO assignments
constexpr uint8_t TACH_PINS[NUM_WHEELS] = {
    29, 30, 31, 32
};

// Replace with actual pulses per wheel revolution (aka teeth per wheel)
constexpr float PULSES_PER_REV[NUM_WHEELS] = {
    24.0f, 24.0f, 24.0f, 24.0f
};

// Update every 20 ms (50 Hz) - change as needed
constexpr uint32_t UPDATE_US = 20000;

// Use multiple pulses for averaging when available
constexpr uint32_t MULTI_PULSE_THRESHOLD = 4;

// Wheel-stop detection limits
constexpr uint32_t MIN_TIMEOUT_US = 40000;
constexpr uint32_t MAX_TIMEOUT_US = 1000000;

// Shared ISR data
volatile uint32_t pulseCount[NUM_WHEELS] = {};
volatile uint32_t firstPulseUs[NUM_WHEELS] = {};
volatile uint32_t lastPulseUs[NUM_WHEELS] = {};
volatile uint32_t lastPeriodUs[NUM_WHEELS] = {};
volatile bool hasPreviousPulse[NUM_WHEELS] = {};

// Calculated values (SEND TO SD CARD AND TELEMETRY)
float wheelHz[NUM_WHEELS] = {};
float wheelRPM[NUM_WHEELS] = {};

uint32_t lastUpdateUs = 0;

// Common interrupt handler
void handleTachPulse(uint8_t wheel) {
    uint32_t now = micros();

    if (hasPreviousPulse[wheel]) {
        lastPeriodUs[wheel] = now - lastPulseUs[wheel];
    } else {
        hasPreviousPulse[wheel] = true;
    }

    lastPulseUs[wheel] = now;

    if (pulseCount[wheel] == 0) {
        firstPulseUs[wheel] = now;
    }

    pulseCount[wheel]++;
}

// Individual ISR wrappers
void isrFL() { handleTachPulse(FL); }
void isrFR() { handleTachPulse(FR); }
void isrRL() { handleTachPulse(RL); }
void isrRR() { handleTachPulse(RR); }

void updateWheelSpeeds() {
    uint32_t counts[NUM_WHEELS];
    uint32_t first[NUM_WHEELS];
    uint32_t last[NUM_WHEELS];
    uint32_t period[NUM_WHEELS];

    // Atomic snapshot of ISR data
    noInterrupts();

    uint32_t now = micros();

    for (uint8_t i = 0; i < NUM_WHEELS; i++) {
        counts[i] = pulseCount[i];
        first[i] = firstPulseUs[i];
        last[i] = lastPulseUs[i];
        period[i] = lastPeriodUs[i];

        pulseCount[i] = 0;
    }

    interrupts();

    lastUpdateUs = now;

    for (uint8_t i = 0; i < NUM_WHEELS; i++) {

        float frequency = 0.0f;

        if (period[i] > 0) {

            uint32_t ageUs = now - last[i];

            // Adaptive timeout: 3 expected pulse periods
            uint64_t timeoutUs =
                static_cast<uint64_t>(period[i]) * 3;

            if (timeoutUs < MIN_TIMEOUT_US)
                timeoutUs = MIN_TIMEOUT_US;

            if (timeoutUs > MAX_TIMEOUT_US)
                timeoutUs = MAX_TIMEOUT_US;

            if (ageUs <= timeoutUs) {

                // High speed: average across several pulses
                if (counts[i] >= MULTI_PULSE_THRESHOLD) {
                    uint32_t spanUs = last[i] - first[i];

                    if (spanUs > 0) {
                        frequency =
                            (counts[i] - 1) * 1000000.0f
                            / spanUs;
                    }
                }

                // Low speed: use most recent pulse period
                else {
                    frequency = 1000000.0f / period[i];
                }
            }
        }

        wheelHz[i] = frequency;

        wheelRPM[i] =
            frequency * 60.0f / PULSES_PER_REV[i];
    }
}

void setup() {
  for (uint8_t i = 0; i < NUM_WHEELS; i++) {
    pinMode(TACH_PINS[i], INPUT);
  }

  attachInterrupt(digitalPinToInterrupt(TACH_PINS[FL]), isrFL, RISING);
  attachInterrupt(digitalPinToInterrupt(TACH_PINS[FR]), isrFR, RISING);
  attachInterrupt(digitalPinToInterrupt(TACH_PINS[RL]), isrRL, RISING);
  attachInterrupt(digitalPinToInterrupt(TACH_PINS[RR]), isrRR, RISING);

  lastUpdateUs = micros();
}

void loop() {
  uint32_t now = micros();

  if ((uint32_t)(now - lastUpdateUs) >= UPDATE_US) {
    updateWheelSpeeds();
  }
}
