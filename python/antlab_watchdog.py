#!/usr/bin/env python3

import os
import socket
import json
import time

WATCHDOG_SOCK = "/tmp/watchdog.sock"
LED_SOCK = "/tmp/led_manager.sock"

GPS_WINDOW_SEC = 3.0
GPS_MIN_PPS_COUNT = 3

CAM_CHECK_SEC = 1.0

CAMERA_LINKS = {
    "cam0": "/dev/cam_center",
    "cam1": "/dev/cam_left",
    "cam2": "/dev/cam_right",
}

pps_count = 0

recv_sock = None
led_sock = None

last_sent = {
    "gps": None,      # "GPS_OK" / "GPS_ERROR"
    "camera": None,   # ("CAM_OK", ()) / ("CAM_ERROR", ("cam1",))
}


def send_msg(source, event, **kwargs):
    global led_sock

    msg = {
        "source": source,
        "event": event,
    }
    msg.update(kwargs)

    try:
        led_sock.sendto(json.dumps(msg).encode(), LED_SOCK)
        print(f"[WATCHDOG] sent: {msg}")
    except FileNotFoundError:
        print("[WATCHDOG] led_manager.sock 이 없음")
    except Exception as e:
        print(f"[WATCHDOG] send error: {e}")


def send_gps_state(event):
    if last_sent["gps"] == event:
        return
    send_msg("trigger", event)
    last_sent["gps"] = event


def send_camera_state(event, missing=None):
    if missing is None:
        missing = []

    state_key = (event, tuple(sorted(missing)))
    if last_sent["camera"] == state_key:
        return

    if event == "CAM_ERROR":
        send_msg("panorama", event, missing=missing)
    else:
        send_msg("panorama", event)

    last_sent["camera"] = state_key


def handle_msg(msg):
    global pps_count

    source = msg.get("source")
    event = msg.get("event")

    if source == "trigger" and event == "PPS_TICK":
        pps_count += 1
        print(f"[WATCHDOG] PPS_TICK received, count={pps_count}")


def check_camera_links():
    missing = []

    for cam_name, link_path in CAMERA_LINKS.items():
        if not os.path.islink(link_path):
            missing.append(cam_name)
            continue

        if not os.path.exists(link_path):
            missing.append(cam_name)
            continue

    return missing


def main():
    global recv_sock
    global led_sock
    global pps_count

    if os.path.exists(WATCHDOG_SOCK):
        os.remove(WATCHDOG_SOCK)

    recv_sock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
    recv_sock.bind(WATCHDOG_SOCK)
    recv_sock.settimeout(0.1)

    led_sock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)

    print(f"[WATCHDOG] listening on {WATCHDOG_SOCK}")

    window_start = time.monotonic()
    last_cam_check = 0.0

    try:
        while True:
            try:
                data, _ = recv_sock.recvfrom(4096)
                msg = json.loads(data.decode())
                handle_msg(msg)
            except socket.timeout:
                pass

            now = time.monotonic()

            if now - window_start >= GPS_WINDOW_SEC:
                count_in_window = pps_count
                print(f"[WATCHDOG] last {GPS_WINDOW_SEC:.1f}s PPS count = {count_in_window}")

                if count_in_window < GPS_MIN_PPS_COUNT:
                    send_gps_state("GPS_ERROR")
                else:
                    send_gps_state("GPS_OK")

                pps_count = 0
                window_start = now

            if now - last_cam_check >= CAM_CHECK_SEC:
                missing = check_camera_links()

                if missing:
                    print(f"[WATCHDOG] missing camera links: {missing}")
                    send_camera_state("CAM_ERROR", missing=missing)
                else:
                    print("[WATCHDOG] all camera links OK")
                    send_camera_state("CAM_OK")

                last_cam_check = now

            time.sleep(0.01)

    except KeyboardInterrupt:
        print("\n[WATCHDOG] 종료")
    finally:
        recv_sock.close()
        led_sock.close()
        if os.path.exists(WATCHDOG_SOCK):
            os.remove(WATCHDOG_SOCK)


if __name__ == "__main__":
    main()
