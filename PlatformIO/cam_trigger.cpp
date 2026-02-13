#include <Arduino.h>

#define PIN_TRIG 25
#define PIN_PPS  32

// 설정값
int period_ms = 100;   // 10Hz
int pulse_us  = 50000;    // 50us 펄스폭
int offset_ms = 0;     // PPS 들어온 뒤 첫 펄스까지 딜레이(원하면 0)

volatile bool pps_flag = false;
volatile unsigned long sec_count = 0;

bool trig_high = false;
unsigned long pulse_end_us = 0;
unsigned long next_trig_ms = 0;

void isr_pps() {
  pps_flag = true;   // ISR에서는 이것만!
  sec_count++;
}

void setup() {
  pinMode(PIN_TRIG, OUTPUT);
  digitalWrite(PIN_TRIG, LOW);

  pinMode(PIN_PPS, INPUT);
  attachInterrupt(digitalPinToInterrupt(PIN_PPS), isr_pps, RISING);

  Serial.begin(115200);
  next_trig_ms = millis();
}

void loop() {
  // 1) PPS 들어오면: 지금 하던 펄스 끊고(LOW), 스케줄 리셋
  if (pps_flag) {
    noInterrupts();
    pps_flag = false;
    unsigned long s = sec_count;
    interrupts();

    //digitalWrite(PIN_TRIG, LOW);
    trig_high = false;

    next_trig_ms = millis() + offset_ms;

    Serial.print("PPS! sec=");
    Serial.println(s);
  }

  unsigned long now_ms = millis();
  unsigned long now_us = micros();

  // 2) 펄스가 HIGH면, pulse_us 지나면 LOW로 내리기
  if (trig_high) {
    if ((long)(now_us - pulse_end_us) >= 0) {
      digitalWrite(PIN_TRIG, LOW);
      trig_high = false;
    }
    return;
  }

  // 3) 10Hz 타이밍이면 HIGH 올리고, pulse_end_us 예약
  if ((long)(now_ms - next_trig_ms) >= 0) {
    digitalWrite(PIN_TRIG, HIGH);
    trig_high = true;
    pulse_end_us = now_us + pulse_us;

    next_trig_ms += period_ms;   // 다음 100ms
  }
}
