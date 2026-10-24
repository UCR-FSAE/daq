
"""Forward newline-delimited LoRa telemetry records to Foxglove.

Expected serial record:
highestTemp=42.5,motorTemp=58.2,motorRPM=3200,dcI=60.00,dcV=359.40

The firmware sends comma-separated key=value pairs, NOT JSON.
"""

import argparse
import time
from datetime import datetime, timezone

import foxglove
import serial

DEFAULT_HOST = "127.0.0.1"
DEFAULT_WS_PORT = 8765

# This is the baud rate of the receiver's computer-facing serial port.
# It is not necessarily the same as the transmitter's UART baud.
DEFAULT_BAUD = 57600

CHANNEL = "/vehicle/telemetry"


def parse_value(value: str):
    """Parse a scalar as an int or float when possible."""
    value = value.strip()

    try:
        return int(value)
    except ValueError:
        pass

    try:
        return float(value)
    except ValueError:
        return value


def decode_packet(raw_line: bytes) -> dict:
    """Decode one comma-separated key=value telemetry record."""
    text = raw_line.decode("utf-8").strip()

    if not text:
        raise ValueError("empty packet")

    packet = {}

    for field in text.split(","):
        key, separator, value = field.partition("=")

        if not separator:
            raise ValueError(f"invalid field: {field!r}")

        key = key.strip()
        value = value.strip()

        if not key or not value:
            raise ValueError(f"empty key or value: {field!r}")

        if key in packet:
            raise ValueError(f"duplicate field: {key}")

        packet[key] = parse_value(value)

    if not packet:
        raise ValueError("packet contains no fields")

    return packet


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Forward LoRa key=value telemetry to Foxglove."
    )
    parser.add_argument(
        "--port",
        required=True,
        help="Receiver serial port, e.g. /dev/cu.usbserial-XXXX or COM5",
    )
    parser.add_argument(
        "--baud",
        type=int,
        default=DEFAULT_BAUD,
        help=f"Receiver serial baud rate (default: {DEFAULT_BAUD})",
    )
    parser.add_argument(
        "--host",
        default=DEFAULT_HOST,
        help="Foxglove WebSocket bind host",
    )
    parser.add_argument(
        "--ws-port",
        type=int,
        default=DEFAULT_WS_PORT,
        help="Foxglove WebSocket port",
    )
    args = parser.parse_args()

    start = time.monotonic()
    seq = 0

    try:
        with serial.Serial(
            args.port,
            args.baud,
            timeout=1,
        ) as receiver:
            # Some serial devices reset when the port opens.
            time.sleep(0.2)
            receiver.reset_input_buffer()

            foxglove.start_server(
                name="FSAE LoRa Telemetry Bridge",
                host=args.host,
                port=args.ws_port,
            )

            print(
                f"Foxglove server: ws://{args.host}:{args.ws_port}",
                flush=True,
            )
            print(
                f"Reading {args.port} at {args.baud} baud",
                flush=True,
            )
            print(f"Publishing to {CHANNEL}", flush=True)

            while True:
                raw_line = receiver.readline()

                if not raw_line:
                    continue

                try:
                    telemetry = decode_packet(raw_line)
                except (UnicodeDecodeError, ValueError) as exc:
                    print(
                        f"Skipping invalid packet: {exc}; "
                        f"raw={raw_line[:160]!r}",
                        flush=True,
                    )
                    continue

                # Bridge-side metadata; do not overwrite firmware fields.
                telemetry.setdefault("seq", seq)
                telemetry["bridge_uptime_ms"] = int(
                    (time.monotonic() - start) * 1000
                )
                telemetry["bridge_timestamp_utc"] = (
                    datetime.now(timezone.utc).isoformat()
                )

                foxglove.log(CHANNEL, telemetry)

                print(
                    f"Forwarded seq={telemetry['seq']}: "
                    f"{telemetry}",
                    flush=True,
                )
                seq += 1

    except KeyboardInterrupt:
        print("\nStopping telemetry bridge.", flush=True)
    except serial.SerialException as exc:
        raise SystemExit(
            f"Could not use receiver serial port {args.port}: {exc}"
        ) from exc


if __name__ == "__main__":
    main()

