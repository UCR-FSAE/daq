import time
import math

import foxglove


def main():
    foxglove.start_server(
        name="FSAE Telemetry",
        host="127.0.0.1",
        port=8765,
    )

    print("Connect Foxglove to ws://127.0.0.1:8765")

    start = time.monotonic()
    seq = 0

    try:
        while True:
            elapsed = time.monotonic() - start

            telemetry = {
                "seq": seq,
                "uptime_ms": int(elapsed * 1000),
                "temp_max_c": 45.0 + 3.0 * math.sin(elapsed),
                "motor_rpm": 3000 + int(500 * math.sin(elapsed / 2)),
                "can_current_a": 60.0 + 10.0 * math.sin(elapsed),
                "rms_current_a": 40.0 + 5.0 * math.sin(elapsed),
                "pack_voltage_v": 360.0 - elapsed * 0.01,
                "rms_voltage_v": 230.0,
                "power_w": 15000.0,
                "soc_pct": 80.0 - elapsed * 0.005,
            }

            foxglove.log("/vehicle/telemetry", telemetry)
            seq += 1
            time.sleep(0.1)

    except KeyboardInterrupt:
        print("Stopping telemetry publisher.")


if __name__ == "__main__":
    main()

