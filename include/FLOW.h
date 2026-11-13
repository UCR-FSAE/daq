#ifndef FLOW_H
#define FLOW_H

#include <Arduino.h>

// Hall-effect turbine flow meter, pulse-counting via hardware interrupt.
// K-factor: pulses per liter (placeholder 450 pulses/L).
class FLOW {
 private:
  uint8_t _pin;
  float _k_factor;
  uint32_t _last_time_ms;
  float _flow_rate_lpm;

 public:
  FLOW(uint8_t pin, float k_factor = 450.0f);
  void begin();
  float read_flow_lpm();
};

#endif // FLOW_H
