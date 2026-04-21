#!/usr/bin/env python3

import socket
import json
import Jetson.GPIO as GPIO
import time

SOCK_PATH = "/tmp/led_manager.sock"
PPS_PIN = 33
WINDOW_SEC = 3.0

pps_count = 0
last_sent_state = None
sock = None


def check(channel):
    global pps_count
    pps_count += 1


def send_msg(event):
    global sock
    global last_sent_state

    # 같은 상태를 계속 중복 전송하지 않도록
    if event == last_sent_state:
        return

    msg = {
        "source": "trigger",
        "event": event,
    }

    try:
        sock.sendto(json.dumps(msg).encode(), SOCK_PATH)
        print(f"[TRIGGER] sent: {msg}")
        last_sent_state = event
    except FileNotFoundError:
        print("[TRIGGER] led_manager.sock 이 없음")
    except Exception as e:
        print(f"[TRIGGER] send error: {e}")


def main():
    global sock
    global pps_count

    sock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)

    GPIO.setmode(GPIO.BOARD)
    GPIO.setup(PPS_PIN, GPIO.IN)

    # 필요하면 bouncetime 추가 가능
    GPIO.add_event_detect(PPS_PIN, GPIO.RISING, callback=check)

    window_start = time.monotonic()

    try:
        while True:
            now = time.monotonic()

            if now - window_start >= WINDOW_SEC:
                count_in_window = pps_count
                print(f"[TRIGGER] last {WINDOW_SEC:.1f}s PPS count = {count_in_window}")

                if count_in_window < 3:
                    send_msg("GPS_ERROR")
                else:
                    send_msg("GPS_OK")

                # 다음 3초 윈도우 시작
                pps_count = 0
                window_start = now

            time.sleep(0.01)

    except KeyboardInterrupt:
        print("\n[TRIGGER] 종료")

    finally:
        GPIO.cleanup()
        sock.close()


if __name__ == "__main__":
    main()
