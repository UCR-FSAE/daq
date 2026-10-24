"""Bridge newline-delimited JSON telemetry from a LoRa receiver to Foxglove.

Expected input: one UTF-8 JSON object per serial line, for example:
{"seq": 105, "motor_temp_c": 58.2, "speed_kph": 42.5, "pack_voltage_v": 359.4, "soc_pct": 78.6}

This file does not simulate values or decode an arbitrary binary radio protocol.
Adapt decode_packet() if your transmitter sends a different packet format.
"""

import argparse
import json
import time
from datetime import datetime, timezone

import foxglove
import serial

DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 8765
DEFAULT_BAUD = 115200
CHANNEL = "/vehicle/telemetry"


def decode_packet(raw_line: bytes) -> dict:
    """Decode one newline-terminated JSON object from the serial receiver."""
    text = raw_line.decode("utf-8").strip()
    if not text:
        raise ValueError("empty packet")

    packet = json.loads(text)
    if not isinstance(packet, dict):
        raise ValueError("packet must be a JSON object")

    return packet


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Forward JSON telemetry from a LoRa receiver to Foxglove."
    )
    parser.add_argument(
        "--port", required=True,
        help="Serial device, e.g. /dev/tty.usbserial-XXXX or COM5",
    )
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD)
    parser.add_argument("--host", default=DEFAULT_HOST,
                        help="Foxglove WebSocket bind host (default: 127.0.0.1)")
    parser.add_argument("--ws-port", type=int, default=DEFAULT_PORT,
                        help="Foxglove WebSocket port (default: 8765)")
    args = parser.parse_args()

    foxglove.start_server(
        name="FSAE LoRa Telemetry Bridge",
        host=args.host,
        port=args.ws_port,
    )
    print(f"Foxglove server started: ws://{args.host}:{args.ws_port}")
    print(f"Reading LoRa receiver serial data from {args.port} at {args.baud} baud")
    print(f"Forwarding decoded packets to {CHANNEL}")
    print("Expected format: one UTF-8 JSON object per line. Press Ctrl+C to stop.")

    seq_fallback = 0
    start = time.monotonic()

    try:
        with serial.Serial(args.port, args.baud, timeout=1) as receiver:
            # Some USB serial adapters reset when opened. Give the device a moment.
            time.sleep(0.2)
            while True:
                raw_line = receiver.readline()
                if not raw_line:
                    continue

                try:
                    telemetry = decode_packet(raw_line)
                except (UnicodeDecodeError, json.JSONDecodeError, ValueError) as exc:
                    print(f"Skipping invalid packet: {exc}; raw={raw_line[:120]!r}")
                    continue

                # Preserve the transmitter's sequence number/timestamps if present.
                # Add bridge-side metadata only when the packet does not contain it.
                telemetry.setdefault("seq", seq_fallback)
                telemetry.setdefault("bridge_uptime_ms", int((time.monotonic() - start) * 1000))
                telemetry.setdefault("bridge_timestamp_utc", datetime.now(timezone.utc).isoformat())

                foxglove.log(CHANNEL, telemetry)

                seq_fallback += 1
                print(f"Forwarded packet seq={telemetry.get('seq')} fields={len(telemetry)}")

    except KeyboardInterrupt:
        print("\nStopping LoRa-to-Foxglove bridge.")
    except serial.SerialException as exc:
        raise SystemExit(f"Could not use serial device {args.port}: {exc}") from exc


if __name__ == "__main__":
    main()
