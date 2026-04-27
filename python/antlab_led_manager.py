#!/usr/bin/env python3

import os
import socket
import json
import Jetson.GPIO as GPIO
import time

SOCK_PATH = "/tmp/led_manager.sock"
GREEN_LED = 15
RED_LED = 29

BLINK_HZ_GPS = 1.0
BLINK_HZ_CAM = 5.0

if os.path.exists(SOCK_PATH):
    os.remove(SOCK_PATH)

sock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
sock.bind(SOCK_PATH)
sock.settimeout(0.01)   # 10ms마다 메시지 확인 후 LED 갱신

print(f"[LED] listening on {SOCK_PATH}")

state = {
    "gps_error": False,
    "camera_error": False,
    "camera_missing": [],
}

mode = "NORMAL"
red_on = False
last_toggle = time.monotonic()


def calc_mode():
    if state["gps_error"] and state["camera_error"]:
        return "BOTH_ERROR"
    elif state["gps_error"]:
        return "GPS_ERROR"
    elif state["camera_error"]:
        return "CAM_ERROR"
    else:
        return "NORMAL"


def set_mode(new_mode):
    global mode
    global red_on
    global last_toggle

    if new_mode != mode:
        print(f"[LED] mode change: {mode} -> {new_mode}")
        mode = new_mode

        # 상태 바뀔 때 점멸 위상 초기화
        red_on = False
        last_toggle = time.monotonic()

        # 상태 전환 순간 기본 출력 정리
        GPIO.output(GREEN_LED, GPIO.LOW)
        GPIO.output(RED_LED, GPIO.LOW)


def handle_msg(msg):
    source = msg.get("source")
    event = msg.get("event")

    print(f"[LED] recv from={source}, event={event}, raw={msg}")

    if source == "trigger":
        if event == "GPS_ERROR":
            state["gps_error"] = True
        elif event == "GPS_OK":
            state["gps_error"] = False

    elif source == "panorama":
        if event == "CAM_ERROR":
            state["camera_error"] = True
            state["camera_missing"] = msg.get("missing", [])
        elif event == "CAM_OK":
            state["camera_error"] = False
            state["camera_missing"] = []

    set_mode(calc_mode())


def blink_red(hz):
    global red_on
    global last_toggle

    now = time.monotonic()
    half_period = 1.0 / (hz * 2.0)

    if now - last_toggle >= half_period:
        red_on = not red_on
        GPIO.output(RED_LED, GPIO.HIGH if red_on else GPIO.LOW)
        last_toggle = now


def update_led():
    if mode == "NORMAL":
        GPIO.output(RED_LED, GPIO.LOW)
        GPIO.output(GREEN_LED, GPIO.HIGH)

    elif mode == "GPS_ERROR":
        GPIO.output(GREEN_LED, GPIO.LOW)
        blink_red(BLINK_HZ_GPS)

    elif mode == "CAM_ERROR":
        GPIO.output(GREEN_LED, GPIO.LOW)
        blink_red(BLINK_HZ_CAM)

    elif mode == "BOTH_ERROR":
        GPIO.output(GREEN_LED, GPIO.LOW)
        GPIO.output(RED_LED, GPIO.HIGH)


def main():
    GPIO.setmode(GPIO.BOARD)
    GPIO.setup(GREEN_LED, GPIO.OUT, initial=GPIO.HIGH)
    GPIO.setup(RED_LED, GPIO.OUT, initial=GPIO.LOW)

    try:
        while True:
            try:
                data, _ = sock.recvfrom(4096)
                msg = json.loads(data.decode())
                handle_msg(msg)
            except socket.timeout:
                pass

            update_led()

    except KeyboardInterrupt:
        print("\n[LED] 종료")

    finally:
        GPIO.output(GREEN_LED, GPIO.LOW)
        GPIO.output(RED_LED, GPIO.LOW)
        GPIO.cleanup()
        sock.close()
        if os.path.exists(SOCK_PATH):
            os.remove(SOCK_PATH)


if __name__ == "__main__":
    main()
