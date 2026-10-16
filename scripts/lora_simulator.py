
import asyncio
import json
import math
import random
import time

import foxglove


# ----------------------------
# Simulation settings
# ----------------------------
HOST = "127.0.0.1"
PORT = 8765

TX_RATE_HZ = 10
PACKET_LOSS_RATE = 0.05       # Drop 5% of transmitted packets
CORRUPTION_RATE = 0.01        # 1% fail packet-format validation
MIN_DELAY_S = 0.02
MAX_DELAY_S = 0.08

OUTAGE_START_S = 20           # First outage begins after 20 seconds
OUTAGE_DURATION_S = 3
OUTAGE_REPEAT_S = 45          # Repeat outages every 45 seconds


async def main():
    foxglove.start_server(
        name="FSAE LoRa Simulator",
        host=HOST,
        port=PORT,
    )

    print(f"Foxglove: ws://{HOST}:{PORT}")
    print("Simulating car TX -> LoRa channel -> ground RX")
    print("Press Ctrl+C to stop.\n")

    start = time.monotonic()
    receiver_queue = asyncio.Queue()

    stats = {
        "sent": 0,
        "received": 0,
        "dropped": 0,
        "corrupted": 0,
    }

    async def transmitter():
        """Generate simulated car telemetry and transmit packets."""
        seq = 0
        interval = 1.0 / TX_RATE_HZ

        while True:
            tick = time.monotonic()
            elapsed = tick - start

            telemetry = {
                "seq": seq,
                "uptime_ms": int(elapsed * 1000),
                "temp_max_c": round(
                    45 + 5 * math.sin(elapsed / 8)
                    + random.uniform(-0.3, 0.3), 2
                ),
                "motor_rpm": int(
                    3000 + 1000 * math.sin(elapsed / 3)
                ),
                "can_current_a": round(
                    60 + 15 * math.sin(elapsed / 4), 2
                ),
                "rms_current_a": round(
                    40 + 8 * math.sin(elapsed / 3), 2
                ),
                "pack_voltage_v": round(360 - 0.02 * elapsed, 2),
                "rms_voltage_v": round(
                    230 + 5 * math.sin(elapsed / 5), 2
                ),
                "power_w": round(
                    15000 + 3000 * math.sin(elapsed / 4), 2
                ),
                "soc_pct": round(max(0, 80 - 0.005 * elapsed), 2),
            }

            # JSON + newline approximates a serial telemetry protocol.
            packet = json.dumps(telemetry).encode("utf-8") + b"\n"
            stats["sent"] += 1

            # Schedule channel delivery without blocking the transmitter.
            asyncio.create_task(
                wireless_channel(packet, elapsed, receiver_queue, stats)
            )

            seq += 1
            await asyncio.sleep(max(0, interval - (time.monotonic() - tick)))

    async def wireless_channel(packet, tx_elapsed, queue, counters):
        """Simulate packet loss, delay, outages, and corrupted packets."""
        if random.random() < PACKET_LOSS_RATE:
            counters["dropped"] += 1
            return

        # Simulate periodic radio-link outages.
        if tx_elapsed >= OUTAGE_START_S:
            since_first_outage = tx_elapsed - OUTAGE_START_S
            in_outage = (
                since_first_outage % OUTAGE_REPEAT_S
                < OUTAGE_DURATION_S
            )
            if in_outage:
                counters["dropped"] += 1
                return

        await asyncio.sleep(random.uniform(MIN_DELAY_S, MAX_DELAY_S))

        # Deliberately damage a few packets to test parser rejection.
        if random.random() < CORRUPTION_RATE:
            packet = packet + b"INVALID"
            counters["corrupted"] += 1

        await queue.put((packet, tx_elapsed))

    async def receiver():
        """Validate received packets and publish them to Foxglove."""
        last_seq = None
        last_rx_time = time.monotonic()

        while True:
            packet, tx_elapsed = await receiver_queue.get()
            rx_time = time.monotonic()

            try:
                # Remove the serial-style newline and parse the JSON.
                telemetry = json.loads(packet.decode("utf-8").strip())
                seq = telemetry["seq"]

                # Basic validation.
                required = [
                    "temp_max_c",
                    "motor_rpm",
                    "can_current_a",
                    "rms_current_a",
                    "pack_voltage_v",
                    "rms_voltage_v",
                    "power_w",
                    "soc_pct",
                ]
                if not all(key in telemetry for key in required):
                    raise ValueError("Missing telemetry field")

                if last_seq is not None and seq <= last_seq:
                    # Delay jitter can reorder packets; discard old packets.
                    print(f"Out-of-order packet rejected: seq={seq}")
                    continue

                if last_seq is not None and seq > last_seq + 1:
                    print(f"Missing {seq - last_seq - 1} packet(s)")

                last_seq = seq
                last_rx_time = rx_time
                counters = stats
                counters["received"] += 1

                sent = counters["sent"]
                loss_pct = (
                    100 * (sent - counters["received"]) / sent
                    if sent else 0
                )

                telemetry["radio_latency_ms"] = round(
                    (rx_time - start - tx_elapsed) * 1000, 1
                )
                telemetry["radio_packets_sent"] = sent
                telemetry["radio_packets_received"] = counters["received"]
                telemetry["radio_packet_loss_pct"] = round(
                    max(0, min(100, loss_pct)), 2
                )
                telemetry["radio_corrupted_packets"] = counters["corrupted"]
                telemetry["radio_signal_ok"] = True

                foxglove.log("/vehicle/telemetry", telemetry)

                print(
                    f"RX seq={seq:05d} | "
                    f"temp={telemetry['temp_max_c']:.1f} C | "
                    f"rpm={telemetry['motor_rpm']} | "
                    f"latency={telemetry['radio_latency_ms']:.1f} ms"
                )

            except (ValueError, UnicodeDecodeError, json.JSONDecodeError, KeyError):
                print("RX rejected malformed packet")
                continue

    async def link_monitor():
        """Publish radio-link health, including during total outages."""
        while True:
            await asyncio.sleep(0.25)
            now = time.monotonic()
            elapsed = now - start

            # Estimate whether the simulated receiver should currently
            # be inside a scheduled outage.
            in_outage = (
                elapsed >= OUTAGE_START_S
                and (elapsed - OUTAGE_START_S) % OUTAGE_REPEAT_S
                < OUTAGE_DURATION_S
            )

            sent = stats["sent"]
            received = stats["received"]
            loss_pct = 100 * (sent - received) / sent if sent else 0

            # The most recent valid receive time is tracked separately below.
            foxglove.log("/radio/status", {
                "simulated_outage": in_outage,
                "packets_sent": sent,
                "packets_received": received,
                "packets_dropped": stats["dropped"],
                "packets_corrupted": stats["corrupted"],
                "packet_loss_pct": round(max(0, min(100, loss_pct)), 2),
            })

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
        await asyncio.gather(*tasks, return_exceptions=True)


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nLoRa simulation stopped.")

