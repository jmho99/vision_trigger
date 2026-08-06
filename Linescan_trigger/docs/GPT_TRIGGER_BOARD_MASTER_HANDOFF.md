# Trigger Board Project Master Handoff

## 문서 정보

| 항목 | 내용 |
|---|---|
| 프로젝트 | Jetson 연동 트리거 보드 |
| 기준 문서 | `TRIGGER_BOARD_HANDOFF.md` |
| 최초 통합 일자 | 2026-07-20 |
| 최종 갱신 일자 | 2026-07-20 |
| 시간 기준 | 대한민국 표준시(KST, UTC+9) |
| 문서 리비전 | `2026-07-20-R01` |
| 관리 방식 | 최신 유효 사양 + 날짜별 누적 변경 이력 |
| 변경 보존 원칙 | 변경·대체·폐기된 내용도 삭제하지 않고 문서 맨 아래 이력에 보존 |

## 0. 문서 관리 규칙

이 문서는 ChatGPT 대화와 Codex 작업 내용을 서로 검토하기 위한 기준 문서다.

1. 본문에는 현재 시점에서 유효한 사양, 구현 상태, 결정 사항을 반영한다.
2. 기존 내용이 변경되더라도 과거 내용을 완전히 삭제하지 않는다.
3. 변경 전 내용은 문서 맨 아래 `변경 이력`에 날짜와 분류를 붙여 보존한다.
4. 변경 이력은 다음 분류를 사용한다.
   - `추가`
   - `변경`
   - `대체됨`
   - `폐기/삭제`
   - `미결정`
   - `검증 결과`
5. 코드로 확인된 내용과 논의 단계의 제안은 반드시 구분한다.
6. Codex가 코드를 수정한 경우 파일명, 함수명, 리비전 또는 커밋 정보를 가능한 범위에서 기록한다.
7. 날짜는 `YYYY-MM-DD` 형식으로 기록한다.
8. 같은 날짜에 여러 차례 변경되면 리비전을 `R01`, `R02`처럼 증가시킨다.
9. 최신 파일명은 `TRIGGER_BOARD_MASTER_HANDOFF.md`로 유지한다.
10. 주요 단계마다 `TRIGGER_BOARD_MASTER_HANDOFF_YYYY-MM-DD.md` 형태의 스냅샷을 함께 보관한다.

## 1. 현재 유효 범위

### 트리거 보드가 담당하는 범위

- Jetson으로부터 USB CDC Serial 패킷을 수신한다.
- 패킷에서 트리거 ON/OFF 상태와 차량 속도를 읽는다.
- 차량 속도를 라인스캔 카메라의 트리거 주파수로 변환한다.
- RGB 카메라와 라인스캔 카메라에 각각 트리거 신호를 출력한다.
- Jetson 내부의 GPS, OBD, 네트워크 수집 및 데이터 융합 로직은 이 문서의 구현 범위에서 제외한다.
- 트리거 보드는 전달받은 최종 속도값과 제어 명령만 사용한다.

### 현재 하드웨어 및 통신 기준

- 보드: Seeed Studio XIAO ESP32-C3
- MCU: Espressif ESP32-C3
- MCU 클럭: 160 MHz
- FPGA: 사용하지 않음
- Jetson 연결: USB CDC Serial
- 통신 속도: 115200 bps
- 속도 전달 단위: 정수 km/h
- 속도 데이터 타입: `uint8_t`
- 기본 패킷 송신 주기: 20 Hz
- 현재 패킷: `[0xAA][flag][speed_kmh][checksum]`

## 2. 현재 유효 트리거 요구사항

### 2.1 RGB 카메라

- 목표 트리거 주파수: 20 Hz
- 주기: 50 ms
- GPS 데이터 갱신 주기와 동일한 속도로 운용
- 별도 GPIO 및 별도 타이머 채널 필요
- 현재 펌웨어에는 미구현
- 펄스 폭, 극성, 입력 전압은 카메라 사양 확인 후 확정

### 2.2 라인스캔 카메라

- 목표: 차량 진행 방향으로 1 mm 이동할 때마다 트리거 1회
- 거리 설정: `1.0 mm/trigger`
- 요구 출력 범위: 최대 56 kHz
- 현재 코드상 보호 한도: 60 kHz
- 실제 운용 한도는 56 kHz로 제한하는 방향
- 현재 출력 핀: XIAO D2 / ESP32-C3 GPIO4
- 현재 구현 방식: ESP32-C3 LEDC, 10-bit, 50% duty PWM
- 고정 펄스 폭 출력 방식은 검토 제안이며 아직 확정·구현되지 않음

트리거 주파수 계산식:

```text
trigger_hz = speed_km_h / 3.6 * (1000 / distance_per_trigger_mm)
```

`distance_per_trigger_mm = 1.0`일 때:

```text
trigger_hz = speed_km_h × 277.777...
```

예시:

| 차량 속도 | 트리거 주파수 |
|---:|---:|
| 3.6 km/h | 1 kHz |
| 36 km/h | 10 kHz |
| 72 km/h | 20 kHz |
| 100 km/h | 약 27.778 kHz |
| 201.6 km/h | 56 kHz |

## 3. 현재 구현 상태와 추가 구현 후보

### 구현됨

- USB CDC Serial 수신
- 4-byte binary packet 파싱
- XOR checksum 확인
- 정수 km/h 속도 수신
- 속도 기반 라인 트리거 주파수 계산
- GPIO4/D2 LEDC PWM 출력
- 속도 0 또는 trigger OFF일 때 라인 트리거 정지
- 최대 60 kHz 코드 제한
- Python 단발/연속 패킷 전송 도구

### 미구현

- RGB 카메라 20 Hz 독립 출력
- USB 통신 timeout/heartbeat에 따른 자동 정지
- 라인스캔 출력의 고정 펄스 폭 방식
- 라인스캔 실제 운용 상한 56 kHz 제한
- 카메라 입력 규격에 맞춘 레벨 시프터 또는 절연 출력
- 저속 영역의 최소 주파수 정책
- 상태 응답 패킷의 정형 binary protocol
- 소수점 속도 입력

### 현재 권장 우선순위

1. 라인스캔 카메라의 트리거 입력 전압, 극성, 최소 HIGH/LOW 시간을 확인한다.
2. RGB 카메라의 트리거 입력 사양과 사용할 GPIO를 확정한다.
3. 실제 운용 상한을 56 kHz로 코드에 반영한다.
4. USB 패킷 timeout을 구현한다.
5. RGB 20 Hz 출력 채널을 추가한다.
6. 오실로스코프로 1 kHz, 10 kHz, 27.778 kHz, 56 kHz를 검증한다.
7. 필요할 경우 50% duty PWM을 고정 폭 펄스로 교체한다.

## 4. Codex와 ChatGPT 간 작업 절차

1. Codex에서 코드 수정 및 빌드·테스트를 수행한다.
2. Codex는 수정한 파일, 함수, 테스트 결과를 이 문서에 반영한다.
3. 최신 `TRIGGER_BOARD_MASTER_HANDOFF.md` 또는 변경된 소스 ZIP을 ChatGPT 대화에 업로드한다.
4. ChatGPT는 요구사항, 계산, 코드와 문서 사이의 불일치를 검토한다.
5. ChatGPT에서 결정되거나 수정된 사항을 이 문서의 본문에 반영한다.
6. 과거 내용은 맨 아래 변경 이력에서 계속 보존한다.
7. 다음 Codex 작업 시 최신 문서를 저장소 `docs/` 아래에 복사해 기준 문서로 사용한다.

권장 저장 위치:

```text
docs/TRIGGER_BOARD_MASTER_HANDOFF.md
docs/history/TRIGGER_BOARD_MASTER_HANDOFF_2026-07-20.md
```

---

# 원본 상세 인수인계 문서

아래 내용은 2026-07-20에 업로드된 `TRIGGER_BOARD_HANDOFF.md`의 원문을 변경 없이 보존한 것이다.

# Trigger Board Handoff

이 문서는 현재까지 논의/구현된 트리거 보드 내용을 다른 ChatGPT 대화나 개발자에게 전달하기 위한 인수인계 문서입니다.

확정 사항은 현재 코드 기준으로 작성했고, 구현되지 않았거나 확인이 필요한 내용은 `미구현` 또는 `추가 확인 필요`로 명확히 표시했습니다.

## 1. MCU/FPGA 종류와 클럭

### 확정

- 보드: Seeed Studio XIAO ESP32-C3
- MCU: Espressif ESP32-C3
- 빌드 타깃: PlatformIO `seeed_xiao_esp32c3`
- 빌드 출력 기준 MCU 클럭: 160 MHz
- FPGA: 사용하지 않음

관련 파일:

- `platformio.ini`
- `include/config.h`

## 2. 현재 구현된 UART 또는 USB CDC 통신 방식

### 확정

현재 기본 통신은 XIAO ESP32-C3의 USB Serial, 즉 USB CDC Serial 방식입니다.

PC/Jetson 입장에서는 COM 포트 또는 `/dev/ttyACM*`, `/dev/ttyUSB*`처럼 보이는 Serial 포트로 사용합니다.

펌웨어에서는 Arduino `Serial` 객체를 사용합니다.

```cpp
Serial.begin(SERIAL_BAUD_RATE);
```

현재 baud rate:

```cpp
constexpr uint32_t SERIAL_BAUD_RATE = 115200;
```

관련 파일/함수:

- `include/config.h`
  - `SERIAL_BAUD_RATE`
- `src/main.cpp`
  - `setup()`
  - `updateSerialInput()`
  - `handleSerialPacket()`

### 미구현

- 외부 UART TX/RX 핀을 사용하는 `Serial1` 통신은 구현되어 있지 않습니다.
- 현재는 USB 케이블 기반 USB CDC Serial 통신입니다.

## 3. UART 명령 프레임 구조

### 확정

현재 명령 프레임은 4 byte 고정 길이 binary packet입니다.

```text
[0xAA][flag][speed_kmh][checksum]
```

| Byte | 필드 | 타입 | 설명 |
|---:|---|---|---|
| 0 | header | `uint8_t` | 항상 `0xAA` |
| 1 | flag | `uint8_t` | trigger/status 동작 |
| 2 | speed_kmh | `uint8_t` | 속도, 0~255 km/h |
| 3 | checksum | `uint8_t` | `header ^ flag ^ speed_kmh` |

Header:

```cpp
constexpr uint8_t SERIAL_PACKET_HEADER = 0xAA;
```

Flag:

| flag | trigger 요청 | status 출력 요청 |
|---:|---|---|
| `1` | ON | OFF |
| `2` | OFF | OFF |
| `3` | ON | ON |
| `4` | OFF | ON |

Checksum:

```text
checksum = 0xAA ^ flag ^ speed_kmh
```

예시:

| 의미 | 패킷 |
|---|---|
| 100 km/h, trigger ON, status OFF | `AA 01 64 CF` |
| 100 km/h, trigger OFF, status OFF | `AA 02 64 CC` |
| 100 km/h, trigger ON, status ON | `AA 03 64 CD` |
| 0 km/h, trigger OFF, status ON | `AA 04 00 AE` |

관련 파일/함수:

- `include/config.h`
  - `SERIAL_PACKET_HEADER`
  - `SERIAL_FLAG_TRIG_ON_STAT_OFF`
  - `SERIAL_FLAG_TRIG_OFF_STAT_OFF`
  - `SERIAL_FLAG_TRIG_ON_STAT_ON`
  - `SERIAL_FLAG_TRIG_OFF_STAT_ON`
- `src/main.cpp`
  - `updateSerialInput()`
  - `handleSerialPacket()`

## 4. Jetson에서 전달되는 속도 데이터의 단위와 갱신 주기

### 확정

속도 데이터 단위:

```text
km/h
```

데이터 타입:

```text
uint8_t, 0~255 km/h
```

현재 Python 운영 스크립트의 기본 송신 주기:

```text
20 Hz
```

즉 기본적으로 50 ms마다 최신 속도 패킷을 전송합니다.

관련 파일/함수:

- `tools/stream_serial_speed.py`
  - `--rate`
  - 기본값 `20.0`
  - `make_packet(speed, trigger, status)`

### 추가 확인 필요

- 실제 Jetson에서 속도를 어떤 소스에서 받을지는 아직 미정입니다.
- 현재 `tools/stream_serial_speed.py`는 콘솔 입력으로 속도를 변경하는 테스트/운영 겸용 구조입니다.
- 나중에 OBD 또는 차량 속도 입력을 붙일 경우 `stream_serial_speed.py`에서 속도 갱신 부분을 Jetson의 실제 입력 함수로 교체하면 됩니다.

## 5. RGB 카메라 트리거 요구사항

### 미구현

RGB 카메라용 별도 트리거 출력은 현재 구현되어 있지 않습니다.

현재 펌웨어는 line-scan camera용 단일 trigger output만 사용합니다.

### 추가 확인 필요

RGB 카메라에 대해 아래 항목 확인이 필요합니다.

| 항목 | 현재 상태 |
|---|---|
| 주파수 | 미정 |
| 펄스 폭 | 미정 |
| 극성 | 미정 |
| 출력 전압 | 미정 |
| 트리거 입력 회로 | 미정 |
| line-scan trigger와 동기화 필요 여부 | 미정 |

현재 코드에는 RGB 카메라 관련 핀, 타이머, 함수가 없습니다.

## 6. 라인스캔 카메라 트리거 요구사항

대상 카메라:

```text
LUCID Vision Triton TRI02KA-M / TRI02KA-MC 계열 line-scan camera
```

### 확정

현재 구현 목표:

```text
차량이 1 mm 이동할 때마다 trigger 1회 출력
```

기본 거리 설정:

```cpp
constexpr float DISTANCE_PER_TRIGGER_MM = 1.0f;
```

최대 카메라 라인레이트 제한:

```cpp
constexpr float MAX_CAMERA_LINE_RATE_HZ = 60000.0f;
```

속도와 트리거 주파수 계산식:

```text
trigger_hz = speed_km_h / 3.6 * (1000 / distance_per_trigger_mm)
```

현재 코드:

```cpp
const float requestedFrequencyHz =
    speedKmh * (1000.0f / 3600.0f) *
    (1000.0f / DISTANCE_PER_TRIGGER_MM);
```

예시:

| 속도 | 거리 조건 | 트리거 주파수 | 주기 |
|---:|---:|---:|---:|
| 10 km/h | 1 mm/trigger | 약 2,778 Hz | 약 360 us |
| 90 km/h | 1 mm/trigger | 약 25,000 Hz | 약 40 us |
| 100 km/h | 1 mm/trigger | 약 27,778 Hz | 약 36 us |

관련 파일/함수:

- `include/config.h`
  - `DISTANCE_PER_TRIGGER_MM`
  - `MAX_CAMERA_LINE_RATE_HZ`
- `src/trigger_output.cpp`
  - `setTriggerFromSpeed()`
  - `triggerFrequencyHz()`
  - `triggerPeriodUs()`

### 최소 및 최대 주파수

최대 주파수:

```text
60000 Hz
```

이는 코드에서 명시적으로 제한합니다.

최소 주파수:

```text
명시적 제한 미구현
```

현재 ESP32-C3 LEDC 설정은 10-bit resolution을 사용합니다. 낮은 주파수에서 `ledc_set_freq()`가 실패할 수 있습니다. 이전 테스트에서 매우 낮은 속도 영역은 실패 사례가 있었으므로, 실제 최소 주파수는 별도 검증이 필요합니다.

추정/주의:

- 현재 10-bit LEDC 설정에서는 수십 Hz 이하에서 실패할 수 있습니다.
- 저속 테스트를 안정적으로 하려면 dynamic LEDC resolution 또는 다른 타이머 방식 검토가 필요합니다.

### 펄스 폭

현재 구현은 고정 pulse width 방식이 아니라 50% duty PWM입니다.

```cpp
constexpr ledc_timer_bit_t LEDC_RESOLUTION = LEDC_TIMER_10_BIT;
constexpr uint32_t HALF_DUTY = 512;
```

즉:

```text
High time = period / 2
Low time = period / 2
```

예:

```text
100 km/h → 약 27,778 Hz → period 약 36 us → high 약 18 us
```

고정 `5 us` pulse 같은 방식은 현재 미구현입니다.

### 극성

현재 출력은 일반 GPIO PWM 기준 active-high로 볼 수 있습니다.

```text
High 구간 = trigger active로 가정
Rising edge trigger 사용 가정
```

단, 실제 카메라 설정에서 `Rising Edge` 또는 `Falling Edge` 중 무엇을 사용하는지는 카메라 설정 확인이 필요합니다.

### 출력 전압

현재 출력은 XIAO ESP32-C3 GPIO 직접 출력입니다.

```text
3.3 V logic
```

카메라 입력이 3.3 V TTL을 직접 받을 수 있는지 확인 필요합니다. 산업용 카메라 입력이 opto-isolated, 5 V, 12 V, 24 V, NPN/PNP 방식일 수 있으므로 필요하면 라인 드라이버/레벨 변환/포토커플러 회로가 필요합니다.

## 7. 차량 정지 시 처리 방식

### 확정

속도가 0이면 trigger 출력을 정지합니다.

관련 코드:

- `src/main.cpp`
  - `applySerialOutput()`

```cpp
if (!requestedTriggerEnabled || latestSerialSpeedKmh == 0) {
  stopTrigger();
  return true;
}
```

- `src/trigger_output.cpp`
  - `stopTrigger()`

```cpp
ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 0);
ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
actualFrequencyHz = 0.0f;
```

상태 출력 예:

```text
OK trig=1 speed=0 km/h hz=0 period_us=0.000 pin=D2(GPIO4)
```

이 경우 `trig=1`은 PC/Jetson의 trigger 요청 상태이고, 실제 출력은 `hz=0`입니다.

## 8. UART 통신 단절 시 안전 동작

### 미구현

현재 USB CDC Serial 통신 단절을 감지하는 timeout/heartbeat 안전 동작은 구현되어 있지 않습니다.

즉, 마지막으로 받은 값이 `100 km/h, trigger ON`이면 이후 UART/USB 통신이 끊겨도 ESP32는 마지막 설정의 PWM을 계속 출력할 수 있습니다.

### 현재 Python 쪽 동작

`tools/stream_serial_speed.py`는 기본적으로 종료 시 trigger off 패킷을 보냅니다.

```text
--stop-on-exit 기본값: true
```

종료 시 trigger off를 보내지 않으려면:

```powershell
python tools\stream_serial_speed.py COM7 --speed 100 --rate 20 --trigger --no-stop-on-exit
```

### 추가 구현 권장

안전 동작이 필요하면 펌웨어에 아래 로직을 추가해야 합니다.

```text
마지막 정상 패킷 수신 시각 저장
일정 시간 동안 패킷 미수신 시 stopTrigger()
예: SERIAL_TIMEOUT_MS = 1000
```

관련 구현 위치 후보:

- `src/main.cpp`
  - `handleSerialPacket()`
  - `loop()`

## 9. 현재 작성된 소스 파일 목록과 역할

### PlatformIO/펌웨어

| 파일 | 역할 |
|---|---|
| `platformio.ini` | PlatformIO 프로젝트 설정, `seeed_xiao_esp32c3`, Arduino framework |
| `include/config.h` | 핀 번호, serial packet 상수, 거리/최대 Hz, CAN enable 설정 |
| `include/trigger_output.h` | trigger 출력 함수 선언 |
| `src/trigger_output.cpp` | ESP32-C3 LEDC PWM 기반 trigger 생성 |
| `include/can_speed.h` | CAN 속도 입력 함수 선언 |
| `src/can_speed.cpp` | TWAI/CAN OBD-II PID 0x0D 속도 요청/파싱 코드, 현재 기본 비활성 |
| `src/main.cpp` | USB Serial packet 수신, 상태 관리, speed→trigger 적용 |

### PC/Jetson Python 도구

| 파일 | 역할 |
|---|---|
| `tools/send_serial_packet.py` | 4 byte binary packet 1회 전송 및 status 응답 1회 읽기 |
| `tools/stream_serial_speed.py` | COM 포트를 계속 열어두고 속도 packet을 주기적으로 전송, 실행 중 속도 변경 가능 |

## 10. 미결정 사항과 추가 확인 필요 항목

### 카메라 관련

- LUCID TRI02KA-M/TRI02KA-MC의 실제 trigger 입력 전압 범위
- trigger 입력 최소 pulse width
- active polarity: rising edge인지 falling edge인지
- `FrameStart`로 운용할지 `LineStart`로 운용할지 최종 결정
- `Height` 설정값
- 카메라 acquisition timeout 또는 stream idle timeout 설정
- trigger가 장시간 끊겼다가 재개될 때 카메라 프로그램에서 acquisition 재시작이 필요한지

### 트리거 보드 관련

- 저속 영역 최소 안정 출력 주파수
- 고정 pulse width 방식 필요 여부
- 50% duty PWM으로 충분한지
- UART/USB CDC 통신 단절 시 자동 정지 timeout 필요 여부
- 속도값을 `uint8_t` 0~255 km/h로 충분히 볼 수 있는지
- 속도 소수점이 필요한지 여부

### Jetson/PC 관련

- Jetson에서 실제 속도를 어떤 소스에서 받을지
- 속도 갱신 주기 최종값
- Python script를 그대로 사용할지, Jetson 메인 애플리케이션에 protocol 송신 코드를 통합할지
- pyserial 사용 가능 여부 및 포트 이름

### RGB 카메라 관련

- RGB 카메라 사용 여부
- RGB 카메라 trigger 주파수
- line-scan trigger와의 동기화 방식
- 별도 출력 핀/타이머 필요 여부

## 11. 현재 코드에서 사용하는 핀 번호와 타이머 채널

### 핀 번호

| 용도 | XIAO pin | ESP32-C3 GPIO | 현재 사용 여부 |
|---|---:|---:|---|
| Line-scan camera trigger output | D2 | GPIO4 | 사용 중 |
| TWAI/CAN TX | D4 | GPIO6 | CAN enable 시 사용 |
| TWAI/CAN RX | D5 | GPIO7 | CAN enable 시 사용 |

관련 파일:

- `include/config.h`

```cpp
constexpr gpio_num_t TRIGGER_GPIO = GPIO_NUM_4;
constexpr gpio_num_t CAN_TX_GPIO = GPIO_NUM_6;
constexpr gpio_num_t CAN_RX_GPIO = GPIO_NUM_7;
```

### LEDC 타이머/채널

관련 파일:

- `src/trigger_output.cpp`

현재 설정:

```cpp
constexpr ledc_mode_t LEDC_MODE = LEDC_LOW_SPEED_MODE;
constexpr ledc_timer_t LEDC_TIMER = LEDC_TIMER_0;
constexpr ledc_channel_t LEDC_CHANNEL = LEDC_CHANNEL_0;
constexpr ledc_timer_bit_t LEDC_RESOLUTION = LEDC_TIMER_10_BIT;
constexpr uint32_t HALF_DUTY = 512;
```

정리:

| 항목 | 값 |
|---|---|
| LEDC mode | `LEDC_LOW_SPEED_MODE` |
| LEDC timer | `LEDC_TIMER_0` |
| LEDC channel | `LEDC_CHANNEL_0` |
| duty resolution | 10-bit |
| duty | 512 / 1024, 약 50% |

## 12. 빌드 방법과 테스트 방법

### 빌드

작업 디렉터리:

```powershell
cd C:\Users\antlab\Desktop\Codex\trigger-board-xiao-esp32c3
```

PlatformIO 빌드:

```powershell
pio run
```

현재 환경에서 `pio`가 PATH에 없으면:

```powershell
& 'C:\Users\antlab\.platformio\penv\Scripts\pio.exe' run
```

### 업로드

PlatformIO upload:

```powershell
pio run -t upload
```

또는 VS Code PlatformIO의 Upload 버튼을 사용합니다.

### 단발 packet 테스트

```powershell
cd C:\Users\antlab\Desktop\Codex\trigger-board-xiao-esp32c3
python tools\send_serial_packet.py COM7 100 --trigger --status
```

예상 출력:

```text
TX: AA 03 64 CD
RX: OK trig=1 speed=100 km/h hz=27778 period_us=36.000 pin=D2(GPIO4)
```

Trigger off:

```powershell
python tools\send_serial_packet.py COM7 0 --no-trigger --status
```

### 연속 전송 테스트

```powershell
python tools\stream_serial_speed.py COM7 --speed 100 --rate 20 --trigger
```

실행 중 명령:

```text
> 90
> 100
> off
> on
> status
> quit
```

종료 시 trigger off를 보내지 않으려면:

```powershell
python tools\stream_serial_speed.py COM7 --speed 100 --rate 20 --trigger --no-stop-on-exit
```

Serial port open 후 보드 reset 또는 첫 packet 누락이 의심되면:

```powershell
python tools\stream_serial_speed.py COM7 --speed 100 --rate 20 --trigger --open-delay 2.0
```

### 측정 포인트

오실로스코프 또는 로직애널라이저로 아래 핀을 측정합니다.

```text
D2 / GPIO4
```

100 km/h, 1 mm/trigger 기준 기대값:

```text
frequency ≈ 27,778 Hz
period ≈ 36 us
high time ≈ 18 us
low time ≈ 18 us
```

## 현재 동작 요약

```text
Jetson/PC
→ USB CDC Serial 4-byte binary packet
→ XIAO ESP32-C3
→ speed_kmh를 trigger_hz로 변환
→ LEDC PWM으로 GPIO4/D2 출력
→ line-scan camera trigger input
```

같은 `speed`와 같은 `trigger on/off` 상태가 반복 수신되면 ESP32는 LEDC 주파수를 재설정하지 않습니다. 속도 또는 trigger 상태가 바뀐 경우에만 PWM 설정을 갱신합니다.

관련 함수:

- `src/main.cpp`
  - `handleSerialPacket()`
  - `applySerialOutput()`
- `src/trigger_output.cpp`
  - `setTriggerFromSpeed()`
  - `stopTrigger()`


---

# 변경 이력

## 2026-07-20 — 추가

- ChatGPT와 Codex가 같은 Markdown 기준 문서를 통해 상호 검토하는 운영 방식을 추가했다.
- 문서에 연도와 일자를 `YYYY-MM-DD` 형식으로 기록하는 규칙을 추가했다.
- 변경·대체·삭제된 내용도 제거하지 않고 문서 맨 아래에서 분류별로 보존하는 규칙을 추가했다.
- RGB 카메라 트리거 목표를 20 Hz, 50 ms 주기로 기록했다.
- 라인스캔 카메라의 요구 최대 주파수를 56 kHz로 기록했다.
- 트리거 보드의 구현 범위를 속도 수신, 주파수 계산, 센서 트리거 출력으로 한정했다.
- Jetson 내부 GPS/OBD 수집·융합 로직은 트리거 보드 구현 범위에서 제외한다고 명시했다.
- 최신 문서와 날짜별 스냅샷을 함께 관리하는 규칙을 추가했다.

## 2026-07-20 — 변경

### 라인스캔 최대 주파수

- 과거 코드 기준:
  - `MAX_CAMERA_LINE_RATE_HZ = 60000.0f`
  - 코드 보호 한도 60 kHz
- 현재 요구사항:
  - 실제 센서 운용 최대값 56 kHz
- 처리 방향:
  - 코드 내부 절대 보호값을 60 kHz로 유지할지, 운용 한도까지 56 kHz로 낮출지는 코드 수정 시 확정한다.
  - 현재 유효 요구사항은 56 kHz다.

### 시스템 설계 범위

- 과거 논의:
  - Jetson에서 GPS/OBD 데이터 수집, 속도 융합, 상태 관리까지 전체 구조를 검토했다.
- 현재 범위:
  - Jetson 내부 로직은 고려 대상에서 제외한다.
  - 트리거 보드는 Jetson이 전달한 속도와 트리거 명령만 처리한다.

### 출력 파형

- 현재 구현:
  - LEDC 기반 50% duty PWM
- 검토 제안:
  - 카메라 입력 최소 펄스 폭에 맞춘 고정 HIGH 폭 펄스
- 상태:
  - 고정 펄스 폭 방식은 아직 확정된 요구사항이 아니며 구현되지 않았다.
  - 카메라 매뉴얼과 실측 결과를 확인한 뒤 결정한다.

## 2026-07-20 — 대체됨

### 라인스캔 주파수 범위

- 초기 대화 내용:
  - 라인스캔 트리거 범위 `1~5 kHz`
- 이후 정정된 요구사항:
  - 라인스캔 트리거 범위 `1~56 kHz`
- 현재 유효값:
  - 최대 56 kHz
- 보존 사유:
  - 요구사항 변경 경위를 추적하기 위해 초기값을 삭제하지 않는다.

## 2026-07-20 — 폐기/삭제

- 현재까지 코드 또는 요구사항에서 완전히 폐기하기로 확정된 항목은 없다.
- 앞으로 삭제가 확정된 코드, 핀, 패킷 필드, 기능은 이 절에 원문과 삭제 사유를 기록한다.

## 2026-07-20 — 미결정

- RGB 카메라 모델
- RGB 트리거 출력 GPIO
- RGB 트리거 펄스 폭
- RGB 트리거 극성
- RGB 트리거 입력 전압
- 라인스캔 트리거 입력 최소 HIGH/LOW 시간
- 라인스캔 트리거 극성
- 라인스캔 카메라의 정확한 입력 전압 및 절연 방식
- 50% duty PWM 유지 여부
- 고정 펄스 폭 적용 여부와 펄스 폭 값
- USB 통신 timeout 값
- 1 kHz 미만 저속 구간에서 정지할지 계속 출력할지
- 속도값을 정수 km/h로 유지할지 소수점 단위로 확장할지
- 코드의 최대 제한을 60 kHz에서 56 kHz로 변경할지 여부

## 2026-07-20 — 검증 결과

- 문서 기준 계산으로 1 mm/trigger일 때 201.6 km/h에서 56 kHz가 된다.
- 현재 `uint8_t speed_kmh`는 201 km/h까지 직접 표현할 수 있으나 201.6 km/h 같은 소수 속도는 표현할 수 없다.
- 202 km/h 입력 시 계산 주파수는 약 56.111 kHz이므로, 실제 요구 최대 56 kHz를 지키려면 출력 clamp가 필요하다.
- 현재 업로드된 인수인계 문서에는 RGB 출력과 통신 timeout이 미구현으로 기록되어 있다.
