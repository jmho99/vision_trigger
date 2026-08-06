import argparse
import sys
import threading
import time

import serial

from send_serial_packet import make_packet


class SharedState:
    def __init__(self, speed: int, trigger: bool) -> None:
        self.lock = threading.Lock()
        self.speed = speed
        self.trigger = trigger
        self.status_once = False
        self.stop = False

    def snapshot(self) -> tuple[int, bool, bool, bool]:
        with self.lock:
            status_once = self.status_once
            self.status_once = False
            return self.speed, self.trigger, status_once, self.stop

    def set_speed(self, speed: int) -> None:
        with self.lock:
            self.speed = speed

    def set_trigger(self, trigger: bool) -> None:
        with self.lock:
            self.trigger = trigger

    def request_status(self) -> None:
        with self.lock:
            self.status_once = True

    def request_stop(self) -> None:
        with self.lock:
            self.stop = True


def clamp_speed(speed: int) -> int:
    return max(0, min(255, speed))


def input_worker(state: SharedState) -> None:
    print("Commands: 0..255=speed, on, off, status, quit")
    while True:
        try:
            text = input("> ").strip().lower()
        except EOFError:
            state.request_stop()
            return

        if not text:
            continue
        if text in ("q", "quit", "exit"):
            state.request_stop()
            return
        if text in ("on", "start", "t1"):
            state.set_trigger(True)
            state.request_status()
            continue
        if text in ("off", "stop", "t0"):
            state.set_trigger(False)
            state.request_status()
            continue
        if text in ("s", "status", "?"):
            state.request_status()
            continue

        try:
            speed = int(text)
        except ValueError:
            print("Invalid command. Use 0..255, on, off, status, quit.")
            continue

        if speed < 0 or speed > 255:
            print("Speed must be 0..255 km/h.")
            continue

        state.set_speed(speed)
        state.request_status()


def open_serial_port(port: str, baud: int, timeout: float) -> serial.Serial:
    ser = serial.Serial()
    ser.port = port
    ser.baudrate = baud
    ser.timeout = timeout
    ser.dtr = False
    ser.rts = False
    ser.open()
    ser.dtr = False
    ser.rts = False
    return ser


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Continuously stream speed packets to the trigger board."
    )
    parser.add_argument("port", help="Serial port, for example COM7")
    parser.add_argument(
        "--speed",
        type=int,
        default=0,
        help="Fixed test speed in km/h, 0..255. Default: 0",
    )
    parser.add_argument(
        "--rate",
        type=float,
        default=20.0,
        help="Send rate in Hz. Default: 20",
    )
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
        help="Enable trigger output while streaming. Default: enabled",
    )
    parser.add_argument(
        "--status-every",
        type=float,
        default=1.0,
        help="Request/read status every N seconds. 0 disables status. Default: 1.0",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=0.05,
        help="Serial read timeout in seconds. Default: 0.05",
    )
    parser.add_argument(
        "--open-delay",
        type=float,
        default=1.2,
        help="Delay after opening serial port, in seconds. Default: 1.2",
    )
    parser.add_argument(
        "--stop-on-exit",
        action=argparse.BooleanOptionalAction,
        default=True,
        help="Send trigger-off packet before exit. Default: enabled",
    )

    args = parser.parse_args()
    if args.rate <= 0:
        raise ValueError("rate must be > 0")

    state = SharedState(clamp_speed(args.speed), args.trigger)
    period_s = 1.0 / args.rate
    next_send_s = time.monotonic()
    last_status_s = 0.0

    with open_serial_port(args.port, args.baud, args.timeout) as ser:
        time.sleep(args.open_delay)
        ser.reset_input_buffer()
        state.request_status()

        print(
            f"Streaming to {args.port} at {args.rate:g} Hz. "
            "Type a new speed while running, or Ctrl+C to stop."
        )
        worker = threading.Thread(target=input_worker, args=(state,), daemon=True)
        worker.start()

        try:
            while True:
                speed, trigger, status_once, should_stop = state.snapshot()
                if should_stop:
                    break

                now_s = time.monotonic()
                if now_s < next_send_s:
                    time.sleep(min(next_send_s - now_s, 0.001))
                    continue

                status = (
                    status_once
                    or
                    args.status_every > 0.0
                    and now_s - last_status_s >= args.status_every
                )
                if status:
                    last_status_s = now_s

                packet = make_packet(speed, trigger, status)
                ser.write(packet)
                ser.flush()

                if status:
                    response = ser.readline()
                    if response:
                        print(response.decode("utf-8", errors="replace").rstrip())
                    else:
                        print("WARN status timeout")

                next_send_s += period_s
                if next_send_s < now_s - period_s:
                    next_send_s = now_s + period_s

        except KeyboardInterrupt:
            pass

        if args.stop_on_exit:
            stop_packet = make_packet(0, False, True)
            ser.write(stop_packet)
            ser.flush()
            response = ser.readline()
            if response:
                print(response.decode("utf-8", errors="replace").rstrip())
            print("Stopped trigger output.")
        else:
            print("Exited without sending trigger-off packet.")

    return 0


if __name__ == "__main__":
    sys.exit(main())
