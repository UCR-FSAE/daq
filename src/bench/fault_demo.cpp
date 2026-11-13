// Bench only (env teensy41_sdbench). Brian's fault-tracking demo timeline, moved out of
// main.cpp during the merge so main.cpp stays clean (Elias's convention). The fake
// handlers are gone: faults now go to the SD log through the real logger hook.
#include <Arduino.h>
#include "faults.h"

// Called every loop() by the bench. Same timeline as Brian's demo, except the IMU
// comes from the fake sensor in sd_bench_main.cpp; GPS is absent (GPS_STALE stays on).
//  5 s: inverter over-current -> logged via 0x0AB, cleared at 6 s
// 15 s: VCU goes silent on CAN -> CAN_VCU_STALE ON ~100 ms later + pit alert
// 17 s: ADC also stops -> ADC_STALE ON
// 19 s: VCU back; 21 s: ADC back
void faultDemoStep(uint32_t t) {
    if (t < 15000 || t >= 19000) last_can_vcu_ms = t;  // VCU silent 15-19 s
    if (t < 17000 || t >= 21000) last_adc_ms = t;      // ADC silent 17-21 s
    last_tc_ms = t;
    last_can_inv_ms = last_can_bms_ms = t;
    // Simulate 0x0AB each loop; real code calls this when CanParser receives 0x0AB.
    uint16_t demo_run_lo = (t >= 5000 && t < 6000) ? 0x0002 : 0;  // byte 4 bit 1 = over-current
    logInverterFaults(0, 0, demo_run_lo, 0, t);
}
