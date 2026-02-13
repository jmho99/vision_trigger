import Jetson.GPIO as GPIO
import time

pps_pin = 7
count = 0
interrupt_state = False

def check(channel):
    global count
    global interrupt_state
    print("Interrupt count: ", count)
    print(time.time())
    count += 1
    interrupt_state = True

def main():

    global interrupt_state
    GPIO.setmode(GPIO.BOARD)
    GPIO.setup(pps_pin, GPIO.IN)

    # By default, the poll time is 0.2 seconds, too
    GPIO.add_event_detect(pps_pin, GPIO.RISING, callback=check, polltime=0.2)
    
    print("Starting demo now! Press CTRL+C to exit")
    print(time.time())
    try:
        while True:
            if interrupt_state == True:
                print("While loop")
                print(time.time())
                interrupt_state = False

    finally:
        GPIO.cleanup()

if __name__ == '__main__':
    main()
