#pragma once
#include <Arduino.h>

// Bench only (env teensy41_sdbench): fake sensors so the logger, health status and
// fault tracking can be tested on a bare Teensy 4.1 (no IMU, wheel sensor, GPS or LoRa).
//
// Serial keys (115200): i = toggle IMU, w = toggle wheel speed, g = toggle fake GPS,
// l = toggle LoRa, v = toggle SIM output (10 Hz sensor lines), a = stop auto script, h = help.
//
// Auto script (BENCH_AUTO_SCRIPT=1, default), on top of Brian's fault timeline:
//  10 s IMU unplugged -> imu STALE, IMU_STALE fault, imu ABSENT ~5 s later
//  20 s IMU plugged back in -> detected within 1 s, OK again
//  25 s LoRa plugged in -> lora OK, pit alerts now "sent"
//  30 s wheel speed stops -> car_speed STALE; 35 s back
// GPS is off by default (GPS-less testing): gps ABSENT/STALE and GPS_STALE stays on.
void benchBegin();
void benchStep(uint32_t now);
