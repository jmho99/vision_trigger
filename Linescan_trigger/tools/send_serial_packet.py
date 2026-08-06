import argparse
import sys
import time

import serial


HEADER = 0xAA

FLAG_TRIG_ON_STAT_OFF = 1
FLAG_TRIG_OFF_STAT_OFF = 2
FLAG_TRIG_ON_STAT_ON = 3
FLAG_TRIG_OFF_STAT_ON = 4


def make_flag(trigger: bool, status: bool) -> int:
    if trigger and status:
        return FLAG_TRIG_ON_STAT_ON
    if trigger:
        return FLAG_TRIG_ON_STAT_OFF
    if status:
        return FLAG_TRIG_OFF_STAT_ON
    return FLAG_TRIG_OFF_STAT_OFF


def make_packet(speed: int, trigger: bool, status: bool) -> bytes:
    if speed < 0 or speed > 255:
        raise ValueError("speed must be 0..255 km/h")

    flag = make_flag(trigger, status)
    checksum = HEADER ^ flag ^ speed
    return bytes([HEADER, flag, speed, checksum])


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Send a 4-byte trigger-board packet and optionally read status."
    )
    parser.add_argument("port", help="Serial port, for example COM7")
    parser.add_argument("speed", type=int, help="Vehicle speed in km/h, 0..255")
    parser.add_argument(
        "--baud",
        type=int,
        default=115200,
        help="Serial baud rate. Default: 115200",
    )
    parser.add_argument(
        "--trigger",
        action=argparse.BooleanOptionalAction,
        default=True,
        help="Enable trigger output. Default: enabled",
    )
    parser.add_argument(
        "--status",
        action=argparse.BooleanOptionalAction,
        default=True,
        help="Request readable status response. Default: enabled",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=1.0,
        help="Read timeout in seconds. Default: 1.0",
    )

    args = parser.parse_args()
    packet = make_packet(args.speed, args.trigger, args.status)

    with serial.Serial(args.port, args.baud, timeout=args.timeout) as ser:
        time.sleep(0.1)
        ser.reset_input_buffer()
        ser.write(packet)
        ser.flush()

        print("TX:", " ".join(f"{byte:02X}" for byte in packet))

        if args.status:
            response = ser.readline()
            if response:
                print("RX:", response.decode("utf-8", errors="replace").rstrip())
            else:
                print("RX: <timeout>")

    return 0


if __name__ == "__main__":
    sys.exit(main())
