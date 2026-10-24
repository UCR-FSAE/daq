import math
import random
import time
from datetime import datetime, timezone

import foxglove


HOST = "127.0.0.1"
PORT = 8765
PUBLISH_HZ = 10.0
PUBLISH_INTERVAL = 1.0 / PUBLISH_HZ


def clamp(value: float, low: float, high: float) -> float:
    return max(low, min(value, high))


def main() -> None:
    foxglove.start_server(
        name="FSAE Telemetry Simulator",
        host=HOST,
        port=PORT,
    )

    print(f"Connect Foxglove to ws://{HOST}:{PORT}")
    print("Publishing synthetic vehicle telemetry at 10 Hz.")
    print("These are simulated values, not real car or LoRa data.")

    start = time.monotonic()
    seq = 0

    try:
        while True:
            now = time.monotonic()
            elapsed = now - start

            # Smooth synthetic signals. These are example ranges, not vehicle limits.
            motor_temp_c = 55.0 + 8.0 * math.sin(elapsed / 18.0)
            inverter_temp_c = 42.0 + 6.0 * math.sin(elapsed / 23.0 + 0.8)

            # Simulate several subpacks and publish the hottest one.
            subpack_temps = [
                34.0 + 5.0 * math.sin(elapsed / 30.0 + phase)
                for phase in (0.0, 0.7, 1.4, 2.1, 2.8, 3.5)
            ]
            highest_subpack_temp_c = max(subpack_temps)

            # Synthetic speed and motor RPM. Not mechanically coupled in this demo.
            speed_kph = clamp(
                45.0 + 35.0 * math.sin(elapsed / 12.0)
                + random.uniform(-1.0, 1.0),
                0.0,
                100.0,
            )
            motor_rpm = max(
                0,
                int(1800 + 2200 * math.sin(elapsed / 12.0)
                    + random.uniform(-40.0, 40.0)),
            )

            # Distinct synthetic current signals.
            can_current_a = max(
                0.0,
                65.0 + 25.0 * math.sin(elapsed / 8.0)
                + random.uniform(-2.0, 2.0),
            )
            rms_current_a = max(
                0.0,
                42.0 + 12.0 * math.sin(elapsed / 8.0 + 0.5)
                + random.uniform(-1.0, 1.0),
            )

            # Synthetic pack and RMS voltages.
            pack_voltage_v = 360.0 - 0.025 * elapsed + 2.0 * math.sin(elapsed / 20.0)
            rms_voltage_v = 230.0 + 4.0 * math.sin(elapsed / 10.0)

            # Use a separate synthetic pack-current signal for DC pack power.
            # In a real system, replace this with the correct battery current field.
            pack_current_a = can_current_a
            power_w = pack_voltage_v * pack_current_a

            # Slow SoC decline, bounded to 0–100%.
            soc_pct = clamp(80.0 - 0.015 * elapsed, 0.0, 100.0)

            telemetry = {
                "seq": seq,
                "uptime_ms": int(elapsed * 1000),
                "timestamp_ms": int(time.time() * 1000),
                "timestamp_utc": datetime.now(timezone.utc).isoformat(),

                "motor_temp_c": round(motor_temp_c, 2),
                "inverter_temp_c": round(inverter_temp_c, 2),
                "highest_subpack_temp_c": round(highest_subpack_temp_c, 2),

                "speed_kph": round(speed_kph, 2),
                "motor_rpm": motor_rpm,

                "can_current_a": round(can_current_a, 2),
                "rms_current_a": round(rms_current_a, 2),

                "pack_voltage_v": round(pack_voltage_v, 2),
                "rms_voltage_v": round(rms_voltage_v, 2),
                "power_w": round(power_w, 2),
                "soc_pct": round(soc_pct, 2),
            }

            foxglove.log("/vehicle/telemetry", telemetry)

            if seq % 10 == 0:
                print(
                    f"seq={seq:6d} | "
                    f"speed={telemetry['speed_kph']:5.1f} km/h | "
                    f"motor={telemetry['motor_temp_c']:5.1f} C | "
                    f"subpack max={telemetry['highest_subpack_temp_c']:5.1f} C | "
                    f"pack={telemetry['pack_voltage_v']:6.1f} V | "
                    f"SoC={telemetry['soc_pct']:5.1f}%"
                )

            seq += 1
            time.sleep(max(0.0, PUBLISH_INTERVAL - (time.monotonic() - now)))

    except KeyboardInterrupt:
        print("\nStopping telemetry simulator.")


if __name__ == "__main__":
    main()
