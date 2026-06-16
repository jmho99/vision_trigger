/**
 라이다-카메라 하드웨어 트리거 코드
 * 요약
  - OS1 LiDAR의 MULTIPURPOSE 출력 신호를 감지하여 EO 카메라 트리거 출력 생성
  - GPTimer를 사용하여 트리거 타이밍 제어
  - GPS PPS 신호를 감지하여 GPS 동기 상태를 판단
 * 조건
  - OS1 MULTIPURPOSE_IO는 OUTPUT_FROM_ENCODER_ANGLE 모드로 설정
  - OS1 sync_pulse_out_angle은 360도로 설정하여 1회전당 1개의 기준 펄스 출력
  - G5 카메라는 외부 하드웨어 트리거 모드로 설정
  - G5 TriggerActivation은 RisingEdge 기준으로 설정
  - GPS PPS는 1Hz 기준의 RisingEdge 신호로 입력
 * 세부사항
  - delay() 함수는 해당 기간 동안 다른 작업이 불가능하여 미사용
  - GPTimer는 ESP32의 하드웨어 타이머로, 설정된 alarm 시간에 콜백 함수를 실행
  - 트리거 타이밍은 LiDAR 펄스 주기의 절반에서 카메라 exposure time 중심을 맞추도록 지연 계산
  - LiDAR 펄스 미수신 시 에러 표시 (LED ON)
  - GPS PPS는 옵션 입력으로 판단하며, 한 번이라도 감지되면 이후부터 감시 대상에 포함
 * 구조
  - LiDAR 펄스 인터럽트: LiDAR 펄스 감지 시 현재 시간과 이전 펄스 시간의 차이를 계산하여 주기 측정, 트리거 타이밍 계산 후 GPTimer 시작
  - GPS PPS 인터럽트: GPS PPS 감지 시 마지막 수신 시간 갱신 및 GPS 감시 대상 등록
  - GPTimer 알람 콜백: 계산된 delayUs 도달 시 EO 트리거 HIGH, 이후 pulse width 도달 시 LOW로 전환
  - LED 제어: LiDAR와 GPS 상태에 따라 ERR LED 점등 또는 점멸 모드로 제어
  - 메인 루프: LiDAR 펄스 미수신 감시 및 상태 표시
 * 트리거 지연 계산식
  - delay_us = (T_lidar / 2) - D_trig2exp_us - (t_exp_us / 2) - O_other_us
  - T_lidar는 OS1 펄스 간격으로 측정한 LiDAR 1회전 주기
  - 위 계산식은 카메라 노광 중심이 LiDAR 기준 180도에 오도록 하기 위함
 * 보정값
  - D_trig2exp_us: G5 트리거 입력에서 카메라 노출 시작까지의 지연, 실측값으로 설정 필요
  - t_exp_us: 카메라 노출 시간, 예시로 1 ms (1000 us) 설정
  - O_other_us: 장착 방향, 케이블, 실험 환경 등에 따른 추가 보정값, 실험적으로 조정 필요
 * 에러 발생 조건
  - LiDAR와 GPS가 모두 미수신 상태이면 ERR LED 점등
  - LiDAR 펄스가 LIDAR_TIMEOUT_US 이상 수신되지 않으면 LiDAR 미수신 에러로 판단
  - GPS PPS가 한 번이라도 감지된 이후 GPS_TIMEOUT_US 이상 수신되지 않으면 GPS 미수신 에러로 판단
  - LiDAR 또는 GPS 중 하나만 미수신 상태이면 ERR LED 점멸 속도로 구분
  - 계산된 delay_us가 0 이하이면 정상 트리거 생성이 불가능하므로 에러 처리한다.
  - 비정상적으로 짧거나 긴 LiDAR 주기는 노이즈 또는 설정 오류로 판단할 수 있다.
 * 단위
  - 모든 시간 단위는 마이크로초(us)로 통일하여 계산 및 GPTimer 설정
  - GPTimer는 1 MHz로 설정하여 1 tick = 1 us로 동작하도록 구성
*/


#include <Arduino.h>
#include "driver/gptimer.h"
#include "driver/gpio.h"

// 핀 설정
#define LiD_PULSE_PIN 3   // OS1 M_IO pulse input, XIAO D1 / GPIO3
#define PPS_PIN       4   // GPS PPS input, XIAO D2 / GPIO4
#define EO_TRIG_PIN   5   // G5 trigger output, XIAO D3 / GPIO5
#define PWR_LED_PIN   10  // Power LED, XIAO D10 / GPIO10
#define ERR_LED_PIN   7   // Error LED, XIAO D5 / GPIO7
uint32_t boot_time_us = 0;

// GPTimer handle 변수
gptimer_handle_t eo_timer = NULL;

// LiDAR 펄스 타이밍
volatile uint32_t last_pulse_us = 0;
volatile uint32_t lidar_period_us = 100000;  // 초기값: 10 Hz = 100 ms
volatile bool period_valid = false;
const uint32_t LIDAR_TIMEOUT_US = 300000;       // 10 Hz 기준 300 ms 이상 미수신 시 error
volatile bool lidar_detected_once = false;
const uint32_t LIDAR_BOOT_GRACE_US = 5000000;   // 부팅 후 5초 동안 LiDAR 미수신 에러 표시 유예
volatile bool invalid_delay_error = false;      // 계산된 delay_us가 0 이하인 경우 에러 상태 플래그
const uint32_t LIDAR_PERIOD_MIN_US = 80000;   // 10 Hz 기준 -20%
const uint32_t LIDAR_PERIOD_MAX_US = 120000;  // 10 Hz 기준 +20%
volatile bool lidar_period_error = false;

// EO 트리거 상태
volatile bool eo_pulse_high_state = false;
volatile bool eo_timer_active = false;

// EO 트리거 보정값, 단위: us
int32_t D_trig2exp_us = 0;     // G5 trigger input -> exposure start 지연, 실측값
int32_t t_exp_us      = 1000;  // exposure time, 예: 1 ms
int32_t O_other_us    = 0;     // 장착 방향/케이블/실험 보정값

// EO 트리거 펄스 폭
const uint32_t EO_PULSE_WIDTH_US = 1000;  // 1 ms

// GPS의 PPS 신호 상태
volatile uint32_t last_gps_pps_us = 0;
volatile bool gps_detected_once = false;
const uint32_t GPS_TIMEOUT_US   = 1500000;      // GPS PPS 1 Hz 기준 1.5초 이상 미수신 시 error

// ERR LED 상태
enum ErrLedMode {
  ERR_LED_OFF = 0,
  ERR_LED_ON,
  ERR_LED_BLINK_SLOW,
  ERR_LED_BLINK_FAST
};

ErrLedMode err_led_mode = ERR_LED_OFF;

uint32_t last_err_led_toggle_ms = 0;
bool err_led_state = false;

// ERR LED 점멸 설정
const uint32_t ERR_BLINK_SLOW_MS = 1000;  // LiDAR 미수신: 느린 점멸
const uint32_t ERR_BLINK_FAST_MS = 250;   // GPS 미수신: 빠른 점멸

// one-shot GPTimer 시작 함수
static inline void IRAM_ATTR start_eo_timer_oneshot(uint32_t delay_us)
{
  if (delay_us < 5) {
    delay_us = 5;  // 너무 짧은 alarm 방지
  }

  // 이미 동작 중이어도 재설정하기 위해 stop 시도
  gptimer_stop(eo_timer);

  gptimer_set_raw_count(eo_timer, 0);

  gptimer_alarm_config_t alarm_config = {
    .alarm_count = delay_us,
    .reload_count = 0,
    .flags = {
      .auto_reload_on_alarm = false,
    },
  };

  gptimer_set_alarm_action(eo_timer, &alarm_config);
  gptimer_start(eo_timer);

  eo_timer_active = true;
}

// GPTimer alarm 콜백 함수
static bool IRAM_ATTR on_eo_timer_alarm(
  gptimer_handle_t timer,
  const gptimer_alarm_event_data_t *edata,
  void *user_ctx
) {
  if (!eo_pulse_high_state) {
    // delayUs 도달 시, EO trigger HIGH
    gpio_set_level((gpio_num_t)EO_TRIG_PIN, 1);
    eo_pulse_high_state = true;

    // pulse width 후 LOW로 내리기 위해 GPTimer 재시작
    start_eo_timer_oneshot(EO_PULSE_WIDTH_US);
  } else {
    // pulse width 도달 시, EO trigger LOW
    gpio_set_level((gpio_num_t)EO_TRIG_PIN, 0);
    eo_pulse_high_state = false;
    eo_timer_active = false;

    gptimer_stop(eo_timer);
  }

  return false;
}

static inline bool IRAM_ATTR is_lidar_period_valid(uint32_t period_us)
{
  return (period_us >= LIDAR_PERIOD_MIN_US &&
          period_us <= LIDAR_PERIOD_MAX_US);
}

static inline int32_t IRAM_ATTR calc_eo_delay_us(uint32_t period_us)
{
  return ((int32_t)period_us / 2)
       - D_trig2exp_us
       - (t_exp_us / 2)
       - O_other_us;
}

static inline void IRAM_ATTR stop_eo_trigger_safely()
{
  gpio_set_level((gpio_num_t)EO_TRIG_PIN, 0);
  gptimer_stop(eo_timer);
  eo_pulse_high_state = false;
  eo_timer_active = false;
}

// LiDAR 신호 인터럽트 콜백 함수
void IRAM_ATTR on_lidar_pulse()
{
  uint32_t now = micros();

  lidar_detected_once = true;

  // 첫 번째 pulse는 주기 계산 불가
  if (last_pulse_us == 0) {
    last_pulse_us = now;
    return;
  }

  uint32_t measured_period = now - last_pulse_us;

  // LiDAR 주기 유효성 검사
  if (!is_lidar_period_valid(measured_period)) {
    lidar_period_error = true;
    period_valid = false;

    // 너무 긴 주기는 LiDAR 누락/재시작 가능성이 있으므로 기준 시간 갱신
    // 너무 짧은 주기는 노이즈 가능성이 높으므로 기준 시간 갱신하지 않음
    if (measured_period > LIDAR_PERIOD_MAX_US) {
      last_pulse_us = now;
    }

    return;
  }

  // 정상 LiDAR pulse
  last_pulse_us = now;
  lidar_period_us = measured_period;
  period_valid = true;
  lidar_period_error = false;

  int32_t delay_us = calc_eo_delay_us(measured_period);

  if (delay_us <= 0) {
    invalid_delay_error = true;
    stop_eo_trigger_safely();
    return;
  }

  invalid_delay_error = false;
  eo_pulse_high_state = false;
  start_eo_timer_oneshot((uint32_t)delay_us);
}

void IRAM_ATTR on_pps_pulse()
{
  last_gps_pps_us = micros();
  gps_detected_once = true;
}

// ERR LED 제어 함수
void update_err_led(ErrLedMode mode)
{
  uint32_t now_ms = millis();

  if (mode != err_led_mode) {
    err_led_mode = mode;
    last_err_led_toggle_ms = now_ms;
    err_led_state = false;
  }

  switch (err_led_mode) {
    case ERR_LED_OFF:
      err_led_state = false;
      digitalWrite(ERR_LED_PIN, LOW);
      break;

    case ERR_LED_ON:
      err_led_state = true;
      digitalWrite(ERR_LED_PIN, HIGH);
      break;

    case ERR_LED_BLINK_SLOW:
      if ((uint32_t)(now_ms - last_err_led_toggle_ms) >= ERR_BLINK_SLOW_MS) {
        last_err_led_toggle_ms = now_ms;
        err_led_state = !err_led_state;
        digitalWrite(ERR_LED_PIN, err_led_state ? HIGH : LOW);
      }
      break;

    case ERR_LED_BLINK_FAST:
      if ((uint32_t)(now_ms - last_err_led_toggle_ms) >= ERR_BLINK_FAST_MS) {
        last_err_led_toggle_ms = now_ms;
        err_led_state = !err_led_state;
        digitalWrite(ERR_LED_PIN, err_led_state ? HIGH : LOW);
      }
      break;
  }
}

void setup()
{
  pinMode(LiD_PULSE_PIN, INPUT);
  pinMode(PPS_PIN, INPUT);
  pinMode(EO_TRIG_PIN, OUTPUT);
  pinMode(PWR_LED_PIN, OUTPUT);
  pinMode(ERR_LED_PIN, OUTPUT);

  digitalWrite(EO_TRIG_PIN, LOW);
  digitalWrite(PWR_LED_PIN, HIGH);
  digitalWrite(ERR_LED_PIN, LOW);

  // GPTimer 생성: 1 MHz = 1 tick = 1 us
  gptimer_config_t timer_config = {
    .clk_src = GPTIMER_CLK_SRC_DEFAULT,
    .direction = GPTIMER_COUNT_UP,
    .resolution_hz = 1000000,
    .flags = {
      .intr_shared = false,
    },
  };

  ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &eo_timer));

  gptimer_event_callbacks_t cbs = {
    .on_alarm = on_eo_timer_alarm,
  };

  ESP_ERROR_CHECK(gptimer_register_event_callbacks(eo_timer, &cbs, NULL));
  ESP_ERROR_CHECK(gptimer_enable(eo_timer));

  attachInterrupt(digitalPinToInterrupt(LiD_PULSE_PIN), on_lidar_pulse, RISING);
  attachInterrupt(digitalPinToInterrupt(PPS_PIN), on_pps_pulse, RISING);
  boot_time_us = micros();
}

void loop()
{
  uint32_t now = micros();

  // 부팅 직후 LiDAR 펄스 수신 전까지 에러 표시 유예
  bool lidar_boot_grace = (now - boot_time_us) < LIDAR_BOOT_GRACE_US;

  // LiDAR는 필수 입력
  bool lidar_alive = lidar_boot_grace ||
                     (lidar_detected_once &&
                     ((uint32_t)(now - last_pulse_us) <= LIDAR_TIMEOUT_US));

  // GPS는 옵션 입력
  // 한 번도 감지되지 않았으면 정상으로 간주
  // 한 번이라도 감지된 이후부터는 timeout 감시
  bool gps_alive = !gps_detected_once ||
                   ((uint32_t)(now - last_gps_pps_us) <= GPS_TIMEOUT_US);

  ErrLedMode mode = ERR_LED_OFF;

  if (!lidar_alive && !gps_alive) {
    mode = ERR_LED_ON;          // LiDAR + GPS 둘 다 미수신
  }
  else if (!lidar_alive) {
    mode = ERR_LED_BLINK_SLOW;  // LiDAR 미수신
  }
  else if (!gps_alive) {
    mode = ERR_LED_BLINK_FAST;  // GPS PPS 미수신
  }
  else {
    mode = ERR_LED_OFF;         // 정상
  }

  update_err_led(mode);

  // LiDAR 미수신 시 EO Trigger는 안전하게 LOW 유지
  if (!lidar_alive) {
    digitalWrite(EO_TRIG_PIN, LOW);
  }
}
