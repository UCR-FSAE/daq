#include "FLOW.h"

// Pulse counter incremented by the ISR. Must be volatile so the compiler
// never caches it — the interrupt can change it at any moment.
static volatile uint32_t _pulse_count = 0;

// ISR: runs in hardware interrupt context. Keep it minimal — just count.
static void _flow_isr() {
  _pulse_count++;
}

FLOW::FLOW(uint8_t pin, float k_factor)
    : _pin(pin), _k_factor(k_factor), _last_time_ms(0), _flow_rate_lpm(0.0f) {}

void FLOW::begin() {
  pinMode(_pin, INPUT_PULLUP);
  // RISING: trigger on the low-to-high edge of each pulse.
  attachInterrupt(digitalPinToInterrupt(_pin), _flow_isr, RISING);
  _last_time_ms = millis();
}

float FLOW::read_flow_lpm() {
  uint32_t now_ms = millis();
  uint32_t elapsed_ms = now_ms - _last_time_ms;

  if (elapsed_ms == 0) return _flow_rate_lpm;

  // Atomically snapshot and reset the counter.
  // noInterrupts/interrupts prevent a pulse from firing between
  // reading and clearing, which would silently drop that count.
  noInterrupts();
  uint32_t count = _pulse_count;
  _pulse_count = 0;
  interrupts();

  _last_time_ms = now_ms;

  // Convert: (pulses / k_factor) gives liters in the window;
  // multiply by (60000 / elapsed_ms) to get liters per minute.
  _flow_rate_lpm = (count * 60000.0f) / (_k_factor * (float)elapsed_ms);
  return _flow_rate_lpm;
}
