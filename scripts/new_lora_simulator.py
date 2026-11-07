
import asyncio
import json
import math
import random
import time

from datetime import datetime, timezone

import foxglove


# --------------------------------------------------
# Server configuration
# --------------------------------------------------

HOST = "127.0.0.1"
PORT = 8765

# --------------------------------------------------
# LoRa simulation settings
# --------------------------------------------------

TX_RATE_HZ = 10

PACKET_LOSS_RATE = 0.05
CORRUPTION_RATE = 0.01

MIN_DELAY_S = 0.02
MAX_DELAY_S = 0.08

OUTAGE_START_S = 20
OUTAGE_DURATION_S = 3
OUTAGE_REPEAT_S = 45

CONNECTION_TIMEOUT_S = 2.0
HEALTH_PUBLISH_INTERVAL_S = 0.25


# --------------------------------------------------
# Vehicle simulation settings
# --------------------------------------------------

# Temperatures in Celsius
BASE_HIGHEST_TEMP = 45.0
BASE_MOTOR_TEMP = 55.0
BASE_INVERTER_TEMP = 50.0

# Electrical values
BASE_DC_V = 360.0
BASE_PACK_VOLTAGE = 360.0
BASE_SYS12V = 12.5

# Speed
MAX_MOTOR_RPM = 6500
MAX_CAR_SPEED = 100.0

# Battery
INITIAL_SOC = 80.0


def generate_vehicle_telemetry(elapsed, seq):
    """
    Generate simulated vehicle measurements.

    Values approximate a running electric race car.
    """

    # Simulate acceleration and deceleration.
    speed_factor = (
        0.5 + 0.5 * math.sin(elapsed / 8)
    )

    car_speed = MAX_CAR_SPEED * speed_factor

    motor_speed = int(
        MAX_MOTOR_RPM * speed_factor
    )

    # Temperature measurements.
    highest_temp = (
        BASE_HIGHEST_TEMP
        + 5 * math.sin(elapsed / 12)
        + random.uniform(-0.3, 0.3)
    )

    motor_temp = (
        BASE_MOTOR_TEMP
        + 8 * math.sin(elapsed / 15)
        + random.uniform(-0.5, 0.5)
    )

    inverter_temp = (
        BASE_INVERTER_TEMP
        + 6 * math.sin(elapsed / 10)
        + random.uniform(-0.4, 0.4)
    )

    # Electrical measurements.
    dc_i = (
        60
        + 30 * speed_factor
        + random.uniform(-1, 1)
    )

    dc_v = (
        BASE_DC_V
        - 0.02 * elapsed
        + random.uniform(-0.5, 0.5)
    )

    out_v = (
        230
        + 5 * math.sin(elapsed / 5)
        + random.uniform(-0.5, 0.5)
    )

    sys12v = (
        BASE_SYS12V
        + 0.1 * math.sin(elapsed / 10)
    )

    # RMS measurements.
    rms_current = (
        40
        + 20 * speed_factor
        + 3 * math.sin(elapsed / 4)
    )

    rms_voltage = (
        230
        + 5 * math.sin(elapsed / 5)
    )

    # Battery measurements.
    pack_voltage = (
        BASE_PACK_VOLTAGE
        - 0.02 * elapsed
    )

    soc = max(
        0,
        INITIAL_SOC - 0.005 * elapsed
    )

    # Approximate electrical power.
    power = dc_v * dc_i

    # Inverter status.
    # 0 = normal, 1 = active.
    inv_lock = 0
    p_fault_lo = 0
    p_fault_hi = 0
    r_fault_lo = 0
    r_fault_hi = 0

    return {
        "seq": seq,

        # General monitoring
        "carSpeed": round(car_speed, 2),
        "soc": round(soc, 2),
        "power": round(power, 2),

        # Temperature monitoring
        "highestTemp": round(highest_temp, 2),
        "motorTemp": round(motor_temp, 2),
        "inverterTemp": round(inverter_temp, 2),

        # Electrical monitoring
        "dcI": round(dc_i, 2),
        "dcV": round(dc_v, 2),
        "outV": round(out_v, 2),
        "sys12V": round(sys12v, 2),

        # Speed
        "motorSpeed": motor_speed,

        # RMS
        "rmsVoltage": round(rms_voltage, 2),
        "rmsCurrent": round(rms_current, 2),

        # Battery
        "packVoltage": round(pack_voltage, 2),

        # Inverter status and faults
        "invLock": inv_lock,
        "pFaultLo": p_fault_lo,
        "pFaultHi": p_fault_hi,
        "rFaultLo": r_fault_lo,
        "rFaultHi": r_fault_hi,
    }


# --------------------------------------------------
# Main simulation
# --------------------------------------------------

async def main():

    foxglove.start_server(
        name="FSAE LoRa Simulator",
        host=HOST,
        port=PORT,
    )

    print(f"Foxglove: ws://{HOST}:{PORT}")
    print("Simulating vehicle -> LoRa -> ground receiver")
    print("Press Ctrl+C to stop.\n")

    start = time.monotonic()

    receiver_queue = asyncio.Queue()

    stats = {
        "sent": 0,
        "received": 0,
        "dropped": 0,
        "corrupted": 0,
    }

    last_rx_time = None
    last_seq = None

    # --------------------------------------------------
    # Wireless outage detection
    # --------------------------------------------------

    def is_in_outage(elapsed):

        if elapsed < OUTAGE_START_S:
            return False

        since_first_outage = elapsed - OUTAGE_START_S

        return (
            since_first_outage % OUTAGE_REPEAT_S
            < OUTAGE_DURATION_S
        )

    # --------------------------------------------------
    # Transmitter
    # --------------------------------------------------

    async def transmitter():
        """
        Simulate the vehicle-side LoRa transmitter.

        Generates a packet at TX_RATE_HZ.
        """

        seq = 0
        interval = 1.0 / TX_RATE_HZ

        while True:

            tick = time.monotonic()
            elapsed = tick - start

            telemetry = generate_vehicle_telemetry(
                elapsed,
                seq,
            )

            # Simulate serial-style JSON packets.
            packet = (
                json.dumps(telemetry).encode("utf-8")
                + b"\n"
            )

            stats["sent"] += 1

            asyncio.create_task(
                wireless_channel(
                    packet,
                    elapsed,
                    receiver_queue,
                )
            )

            seq += 1

            await asyncio.sleep(
                max(
                    0,
                    interval - (time.monotonic() - tick),
                )
            )

    # --------------------------------------------------
    # Wireless channel
    # --------------------------------------------------

    async def wireless_channel(
        packet,
        tx_elapsed,
        queue,
    ):
        """
        Simulate LoRa transmission.

        Includes:
        - Random packet loss
        - Scheduled outages
        - Variable transmission delay
        - Packet corruption
        """

        if random.random() < PACKET_LOSS_RATE:

            stats["dropped"] += 1
            return

        if is_in_outage(tx_elapsed):

            stats["dropped"] += 1
            return

        await asyncio.sleep(
            random.uniform(
                MIN_DELAY_S,
                MAX_DELAY_S,
            )
        )

        if random.random() < CORRUPTION_RATE:

            packet += b"INVALID"
            stats["corrupted"] += 1

        await queue.put(
            (packet, tx_elapsed)
        )

    # --------------------------------------------------
    # Receiver
    # --------------------------------------------------

    async def receiver():
        """
        Simulate the ground-side LoRa receiver.

        Validates packets and publishes vehicle
        telemetry to Foxglove.
        """

        nonlocal last_rx_time, last_seq

        required_fields = [
            "carSpeed",
            "soc",
            "power",
            "highestTemp",
            "motorTemp",
            "inverterTemp",
            "dcI",
            "dcV",
            "outV",
            "sys12V",
            "motorSpeed",
            "rmsVoltage",
            "rmsCurrent",
            "packVoltage",
            "invLock",
            "pFaultLo",
            "pFaultHi",
            "rFaultLo",
            "rFaultHi",
        ]

        while True:

            packet, tx_elapsed = await receiver_queue.get()

            rx_time = time.monotonic()

            try:

                telemetry = json.loads(
                    packet.decode("utf-8").strip()
                )

                seq = telemetry["seq"]

                # Validate required fields.
                if not all(
                    key in telemetry
                    for key in required_fields
                ):
                    raise ValueError(
                        "Missing telemetry field"
                    )

                # Reject out-of-order packets.
                if last_seq is not None and seq <= last_seq:

                    print(
                        f"Out-of-order packet rejected: {seq}"
                    )
                    continue

                # Detect missing packets.
                if last_seq is not None and seq > last_seq + 1:

                    missing = seq - last_seq - 1

                    print(
                        f"Missing {missing} packet(s)"
                    )

                last_seq = seq
                last_rx_time = rx_time

                stats["received"] += 1

                # --------------------------------------------------
                # Vehicle telemetry topic
                # --------------------------------------------------

                telemetry["radio_latency_ms"] = round(
                    (rx_time - start - tx_elapsed) * 1000,
                    1,
                )

                foxglove.log(
                    "/vehicle/telemetry",
                    telemetry,
                )

                # --------------------------------------------------
                # Print receiver output
                # --------------------------------------------------

                print(
                    f"RX seq={seq:05d} | "
                    f"speed={telemetry['carSpeed']:.1f} km/h | "
                    f"motor={telemetry['motorSpeed']} RPM | "
                    f"temp={telemetry['highestTemp']:.1f} C | "
                    f"DC I={telemetry['dcI']:.1f} A | "
                    f"latency={telemetry['radio_latency_ms']:.1f} ms"
                )

            except (
                ValueError,
                UnicodeDecodeError,
                json.JSONDecodeError,
                KeyError,
            ):

                print("RX rejected malformed packet")

    # --------------------------------------------------
    # Telemetry connection health
    # --------------------------------------------------

    async def link_monitor():
        """
        Publish radio and receiver health separately
        from vehicle measurements.
        """

        while True:

            await asyncio.sleep(
                HEALTH_PUBLISH_INTERVAL_S
            )

            now = time.monotonic()
            elapsed = now - start

            sent = stats["sent"]
            received = stats["received"]

            loss_pct = (
                100 * (sent - received) / sent
                if sent else 0
            )

            # Determine connection state using the
            # time since the last valid packet.
            connected = (
                last_rx_time is not None
                and now - last_rx_time < CONNECTION_TIMEOUT_S
            )

            in_outage = is_in_outage(elapsed)

            # Timestamp of the most recently received packet.
            timestamp = (
                datetime.fromtimestamp(
                    time.time(),
                    tz=timezone.utc,
                ).isoformat()
                if connected else None
            )

            foxglove.log(
                "/vehicle/telemetry_health",
                {
                    "seq": (
                        last_seq
                        if last_seq is not None
                        else 281
                    ),

                    "bridge_uptime_ms": int(
                        elapsed * 1000
                    ),

                    "bridge_timestamp_utc": timestamp,

                    "connected": connected,

                    "radio_latency_ms": (
                        round(
                            (now - last_rx_time) * 1000,
                            1,
                        )
                        if last_rx_time is not None
                        else -1
                    ),

                    "packets_sent": sent,
                    "packets_received": received,
                    "packets_dropped": stats["dropped"],
                    "packets_corrupted": stats["corrupted"],

                    "packet_loss_pct": round(
                        max(0, min(100, loss_pct)),
                        2,
                    ),

                    "simulated_outage": in_outage,
                },
            )

    # --------------------------------------------------
    # Run simulation
    # --------------------------------------------------

    tasks = [
        asyncio.create_task(transmitter()),
        asyncio.create_task(receiver()),
        asyncio.create_task(link_monitor()),
    ]

    try:

        await asyncio.gather(*tasks)

    finally:

        for task in tasks:
            task.cancel()

        await asyncio.gather(
            *tasks,
            return_exceptions=True,
        )


# --------------------------------------------------
# Entry point
# --------------------------------------------------

if __name__ == "__main__":

    try:
        asyncio.run(main())

    except KeyboardInterrupt:
        print("\nLoRa simulation stopped.")

