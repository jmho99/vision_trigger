import Jetson.GPIO as GPIO
import time

pps_pin = 7
cam_pin = 12
period_us = 100000 #100ms, 10Hz
pulse_width_us = 50000 #50ms

count = 0
pps_flag = False
trig_high = False

pulse_end_us = 0
next_trig_us = 0

def check(channel):
    global count
    global pps_flag
    print("Interrupt count: ", count)
    print(time.time())
    count += 1
    pps_flag = True

def main():

    global pps_flag
    global trig_high
    global pulse_end_us
    global next_trig_us
    
    GPIO.setmode(GPIO.BOARD)
    GPIO.setup(pps_pin, GPIO.IN)
    GPIO.setup(cam_pin, GPIO.OUT)

    # By default, the poll time is 0.2 seconds, too
    GPIO.add_event_detect(pps_pin, GPIO.RISING, callback=check, polltime=0.2)
    
    next_trig_us = time.time() * 1000000
    
    print("Starting demo now! Press CTRL+C to exit")
    print(time.time())
    try:
        while True:
            if pps_flag == True:
                print("While loop")
                print(time.time())
                pps_flag = False
                trig_high = False
                next_trig_us = time.time() * 1000000
                
            now = time.time() * 1000000
            
            if trig_high == True:
                if now - pulse_end_us >= 0:
                    print("GPIO LOW", time.time())
                    GPIO.output(cam_pin,GPIO.LOW)
                    trig_high = False
                    
            if now - next_trig_us >= 0:
                print("GPIO HIGH : ", time.time())
                GPIO.output(cam_pin,GPIO.HIGH)
                trig_high = True
                pulse_end_us = now + pulse_width_us
                
                next_trig_us = next_trig_us + period_us

    finally:
        GPIO.cleanup()

if __name__ == '__main__':
    main()
