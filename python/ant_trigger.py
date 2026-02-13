import Jetson.GPIO as GPIO
import time

# Pin Definitions
output_pin = 7  # BCM pin 18, BOARD pin 12

def main():
    # Pin Setup:
    GPIO.setmode(GPIO.BOARD)  # BCM pin-numbering scheme from Raspberry Pi
    # set pin as an output pin with optional initial state of HIGH
    GPIO.setup(output_pin, GPIO.OUT, initial=GPIO.LOW)

    print("Starting demo now! Press CTRL+C to exit")
    try:
        while True:
            
            print("Outputting pin {}".format(output_pin))
            GPIO.output(output_pin, GPIO.HIGH)
            time.sleep(0.05)
            GPIO.output(output_pin, GPIO.LOW)
            time.sleep(0.05)
    finally:
        GPIO.cleanup()

if __name__ == '__main__':
    main()
