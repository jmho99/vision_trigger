#!/usr/bin/env python3

import socket
import json
import time
import Jetson.GPIO as GPIO

WATCHDOG_SOCK = "/tmp/watchdog.sock"
PPS_PIN = 31   # 실제 PPS 입력 핀으로 수정

sock = None

CAM_PIN = 33
period_us = 100000 #100000 #100ms, 10Hz #33333
pulse_width_us = 50000 #50000 #50ms #16666

pps_flag = False
trig_high = False

pulse_end_us = 0
next_trig_us = 0

def send_pps_tick():
    global sock
    msg = {
        "source": "trigger",
        "event": "PPS_TICK",
        "ts": time.monotonic(),
    }
    try:
        sock.sendto(json.dumps(msg).encode(), WATCHDOG_SOCK)
        print(f"[TRIGGER] sent: {msg}")
    except FileNotFoundError:
        print("[TRIGGER] watchdog.sock 이 없음")
    except Exception as e:
        print(f"[TRIGGER] send error: {e}")

def pps_callback(channel):
    send_pps_tick()
    global pps_flag
    pps_flag = True

def main():
    global sock
    global pps_flag
    global trig_high
    global pulse_end_us
    global next_trig_us
    

    sock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)

    GPIO.setmode(GPIO.BOARD)
    GPIO.setup(PPS_PIN, GPIO.IN)
    GPIO.setup(CAM_PIN, GPIO.OUT)
    GPIO.add_event_detect(PPS_PIN, GPIO.RISING, callback=pps_callback)
    GPIO.output(CAM_PIN,GPIO.LOW)

    print("[TRIGGER] PPS interrupt waiting...")
    next_trig_us = time.time() * 1000000

    try:
        while True:
            if pps_flag == True:
                pps_flag = False
                print("PPS in GPIO LOW", time.time())
                GPIO.output(CAM_PIN,GPIO.LOW)
                trig_high = False
                
                next_trig_us = time.time() * 1000000
                
            now = time.time() * 1000000
            
            if trig_high == True:
                if now - pulse_end_us >= 0:
                    print("GPIO LOW", time.time())
                    GPIO.output(CAM_PIN,GPIO.LOW)
                    trig_high = False
                    
            if now - next_trig_us >= 0:
                print("GPIO HIGH : ", time.time())
                GPIO.output(CAM_PIN,GPIO.HIGH)
                trig_high = True
                pulse_end_us = now + pulse_width_us
                
                next_trig_us = next_trig_us + period_us

    except KeyboardInterrupt:
        print("\n[TRIGGER] 종료")
    finally:
        GPIO.cleanup()
        sock.close()

if __name__ == "__main__":
    main()
