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

## 문서 업데이트 원칙

작성 기준일:

```text
2026-07-20
```

앞으로 트리거 보드 관련 대화, 코드 변경, 카메라 테스트 결과, 프로토콜 변경, 미구현 항목 확정이 생기면 이 문서를 계속 갱신합니다.

원칙:

- 확정된 최신 내용은 해당 본문 섹션에 반영합니다.
- 이전 내용이 삭제되거나 변경되더라도 완전히 제거하지 않고, 아래 이력 섹션에 날짜와 함께 남깁니다.
- 추정, 미구현, 추가 확인 필요 항목은 확정 사항과 분리해서 표시합니다.
- 가능하면 변경 날짜는 `YYYY-MM-DD` 형식으로 기록합니다.
- 코드 변경이 있으면 관련 파일명과 함수명을 함께 기록합니다.

## 변경 이력

### 2026-07-20

#### 추가

- `docs/TRIGGER_BOARD_HANDOFF.md` 최초 작성.
- 현재 구현된 USB CDC Serial 4-byte binary packet 구조 문서화.
- XIAO ESP32-C3 / ESP32-C3 160 MHz / LEDC Timer0 Channel0 / GPIO4 출력 구조 문서화.
- Python 단발 전송 스크립트와 연속 전송 스크립트 역할 문서화.

#### 변경

- 없음.

#### 삭제 또는 폐기된 내용

- 없음.

#### 미구현으로 명시한 내용

- RGB 카메라 trigger 출력.
- USB/UART 통신 단절 시 펌웨어 자동 정지 timeout.
- 외부 UART `Serial1` 통신.
- 고정 pulse width trigger 출력.

#### 추가 확인 필요로 남긴 내용

- LUCID TRI02KA-M/TRI02KA-MC trigger 입력 전압.
- trigger 최소 pulse width.
- trigger 극성.
- `FrameStart`/`LineStart` 최종 운용 방식.
- 카메라 acquisition/stream timeout 설정.
- Jetson 실제 속도 입력 소스와 갱신 주기.
## 2026-07-20-R02 Codex 추가 업데이트

### 확정 또는 유지

- 라인스캔 카메라 최대 보호 주파수는 현재 코드값 `60000.0f`를 유지한다.
  - 관련 파일: `include/config.h`
  - 관련 상수: `MAX_CAMERA_LINE_RATE_HZ`
- Jetson은 보정/융합이 끝난 최종 차량 속도값만 트리거 보드로 전달한다.
  - 트리거 보드는 GPS/OBD/센서 융합 로직을 수행하지 않는다.
  - 트리거 보드는 전달받은 최종 속도값을 기준으로 트리거 주파수만 계산한다.

### 변경 또는 요구사항 구체화

- RGB 카메라도 트리거 보드에서 생성하는 방향으로 요구사항을 변경한다.
- 목적은 라인스캔 카메라와 RGB 카메라가 동일 장면을 함께 촬영하도록 동기화하는 것이다.
- 다만 RGB 카메라와 라인스캔 카메라가 같은 주파수의 트리거를 요구하는지는 아직 확인되지 않았다.

### 기술 검토 메모

- 라인스캔 카메라는 차량 속도에 따라 kHz~수십 kHz 트리거가 필요하다.
- RGB 카메라가 일반 area-scan 카메라라면 보통 라인스캔과 같은 kHz 트리거를 직접 받을 가능성은 낮다.
- 따라서 “같이 촬영”은 가능하지만, 반드시 동일 timer/동일 주파수를 의미하지는 않는다.
- RGB 트리거가 20 Hz 등 저주파이고 라인스캔 트리거가 수 kHz~수십 kHz라면 동일 LEDC timer를 공유할 수 없다.
- 같은 timer를 쓰려면 두 출력이 같은 주파수와 같은 phase/duty 조건을 가져야 한다.
- RGB 동기화는 아래 방식 중 하나로 결정해야 한다.
  - 별도 timer/channel로 RGB trigger 생성
  - 라인스캔 트리거 count를 기준으로 N line마다 RGB trigger 1회 생성
  - Jetson 명령으로 라인스캔/RGB trigger enable을 동시에 제어

### LEDC 관련 정정

- 현재 코드의 LEDC 출력은 고정 pulse width가 아니라 50% duty PWM이다.
  - 관련 파일: `src/trigger_output.cpp`
  - 관련 함수: `setTriggerFromSpeed()`
  - 관련 상수: `HALF_DUTY = 512`, `LEDC_RESOLUTION = LEDC_TIMER_10_BIT`
- LEDC는 PWM 주파수와 duty 값을 설정하는 주변장치이다.
- “고정 pulse width”가 필요하면 현재 구현처럼 duty를 50%로 고정하면 안 된다.
- 고정 high time을 LEDC로 흉내 내려면 주파수가 바뀔 때마다 duty 값을 다시 계산해야 한다.
- 더 정확한 고정 pulse width가 필요하면 ESP32 timer interrupt, RMT, GPTimer 기반 one-shot/pulse generator 방식 검토가 필요하다.

### 미구현

- RGB 카메라 trigger 출력 핀.
- RGB 카메라 trigger timer/channel.
- RGB 카메라 trigger 주파수, pulse width, 극성, 출력 전압.
- 라인스캔과 RGB의 동기화 방식.
- 고정 pulse width 출력 방식.

### 추가 확인 필요

- RGB 카메라 모델명과 trigger 입력 사양.
- RGB 카메라가 요구하는 trigger 주파수.
- RGB 카메라가 라인스캔과 동일한 trigger 입력을 받을 수 있는지.
- RGB와 라인스캔을 완전 동일 pulse로 구동할지, 같은 시점 기준으로 별도 주파수 trigger를 생성할지.
- 현재 50% duty PWM이 카메라 입력 조건을 만족하는지.
## 2026-07-20-R03 Codex 추가 업데이트

### 확정

- RGB 카메라는 `AcquisitionStart` trigger로 동작시키는 방향이다.
- 라인스캔 카메라는 `LineStart` trigger로 동작시키는 방향이다.
- 두 카메라의 trigger 주파수가 서로 다르므로 별도 timer/channel로 동작시켜야 한다.
- 현재 확인 기준으로 RGB/라인스캔 모두 trigger pulse width는 크게 민감하지 않다.

### 구현 방향

- 라인스캔 trigger:
  - 차량 속도 기반 가변 주파수.
  - 현재 구현: ESP32-C3 LEDC timer0/channel0, D2/GPIO4.
  - trigger selector: `LineStart`.
- RGB trigger:
  - 별도 timer/channel 필요.
  - trigger selector: `AcquisitionStart`.
  - 목표 주파수는 별도 확정 필요. 이전 문서에는 20 Hz 후보가 있음.

### 고정 pulse width 관련 결정

- 현재는 펄스 폭이 크게 문제 되지 않는 것으로 판단한다.
- 따라서 당장 고정 pulse width 방식으로 변경하지 않는다.
- 현재 50% duty PWM 방식을 유지해도 된다.
- 단, 향후 아래 문제가 생기면 고정 pulse width 방식으로 변경을 재검토한다.
  - 카메라가 특정 최소/최대 high time을 요구하는 경우
  - 고속에서 50% duty high/low 시간이 너무 짧거나 길어지는 경우
  - RGB와 라인스캔 trigger 입력 회로가 서로 다른 pulse 조건을 요구하는 경우

### 미구현

- RGB `AcquisitionStart` trigger 출력.
- RGB용 GPIO.
- RGB용 별도 timer/channel.
- RGB trigger 주파수 설정.
- 라인스캔/RGB 동시 start/stop 제어 로직.

### 추가 확인 필요

- RGB 카메라 목표 frame trigger 주파수.
- RGB 카메라 trigger 입력 전압/극성.
- RGB 카메라가 `AcquisitionStart` 이후 연속 촬영인지, 매 프레임마다 trigger가 필요한지.
- 라인스캔 `LineStart` 설정에서 현재 50% duty PWM이 안정적으로 인식되는지.
## 2026-07-20-R04 Codex 코드 변경 업데이트

### 코드 변경 요약

- `main.cpp`를 packet 수신/상태 출력/전체 trigger board 제어 중심으로 재작성했다.
- 라인스캔 카메라와 RGB 카메라 trigger 출력을 별도 모듈로 분리했다.
- ESP32-C3 LEDC 같은 보드 특수 기능은 `esp32_ledc_pwm_output.*`로 분리했다.
- USB CDC 4-byte binary packet parser는 `trigger_protocol.*`로 분리했다.
- 속도 입력 외에 Ouster LiDAR angle, PPS, external pulse 같은 다른 입력 소스를 고려해 `trigger_types.h`에 입력 종류 enum을 추가했다.
- heap 단편화를 피하기 위해 `fixed_arena.h`를 추가했고, `main.cpp`에서 `TriggerSerialPacketReader`를 fixed arena에 placement-new로 생성한다.

### 추가된 파일

- `include/fixed_arena.h`
  - 고정 크기 arena allocator.
  - 동적 heap 사용 없이 런타임 객체를 배치하기 위한 모듈.
- `include/trigger_types.h`
  - `TriggerInputKind`
  - `TriggerInputSample`
  - `TriggerCommand`
- `include/trigger_protocol.h`
- `src/trigger_protocol.cpp`
  - 4-byte serial packet streaming reader/parser.
- `include/esp32_ledc_pwm_output.h`
- `src/esp32_ledc_pwm_output.cpp`
  - ESP32-C3 LEDC PWM backend.
  - 다른 보드로 이식 시 이 파일을 교체하는 방향.
- `include/line_scan_trigger.h`
- `src/line_scan_trigger.cpp`
  - 라인스캔 카메라 `LineStart` trigger.
  - 차량 속도 기반 가변 주파수.
- `include/rgb_camera_trigger.h`
- `src/rgb_camera_trigger.cpp`
  - RGB 카메라 `AcquisitionStart` trigger.
  - 현재 기본값은 `RGB_CAMERA_TRIGGER_HZ = 20.0f`.
- `include/trigger_board.h`
- `src/trigger_board.cpp`
  - line-scan/RGB trigger 동시 제어.

### 변경된 파일

- `include/config.h`
  - `TRIGGER_GPIO`를 `LINE_SCAN_TRIGGER_GPIO`로 분리.
  - `RGB_CAMERA_TRIGGER_GPIO = GPIO_NUM_5` 추가.
  - `RGB_CAMERA_TRIGGER_HZ = 20.0f` 추가.
- `src/main.cpp`
  - 기존 line-scan 단일 출력 구조에서 `trigger_board` 기반 구조로 변경.
  - status 출력이 line/RGB 각각의 Hz와 period를 표시하도록 변경.
- `src/trigger_output.cpp`
  - 기존 API 호환용 line-scan wrapper로 변경.
- `README.md`
  - RGB/line-scan 분리, source structure, status 출력 형식 반영.

### 현재 핀/타이머 배정

| 카메라 | Trigger selector | XIAO pin | GPIO | LEDC timer | LEDC channel | 비고 |
|---|---|---:|---:|---|---|---|
| Line-scan | `LineStart` | D2 | GPIO4 | `LEDC_TIMER_0` | `LEDC_CHANNEL_0` | 속도 기반 가변 Hz |
| RGB | `AcquisitionStart` | D3 | GPIO5 | `LEDC_TIMER_1` | `LEDC_CHANNEL_1` | 현재 20 Hz 기본값 |

### 현재 출력 방식

- 두 출력 모두 현재는 50% duty PWM이다.
- 고정 pulse width 방식은 아직 구현하지 않았다.
- 사용자가 현재 pulse width는 크게 상관없다고 판단했으므로 당장 변경하지 않는다.

### 빌드 검증

2026-07-20 기준 PlatformIO 빌드 성공.

```text
pio run
SUCCESS
```

빌드 결과:

```text
RAM:   4.2%
Flash: 21.4%
```

### 미구현 또는 추가 확인 필요

- RGB trigger frequency 최종값.
- RGB trigger가 `AcquisitionStart` 1회 pulse인지, 주기적 acquisition start pulse인지 최종 확인.
- RGB 출력 GPIO D3/GPIO5 사용 가능성 현장 확인.
- UART/USB CDC 통신 단절 시 timeout 자동 정지.
- ESP32 외 Jetson/Raspberry Pi/Arduino 이식용 backend.
- Ouster LiDAR angle/PPS/external pulse 입력의 실제 변환 로직.
## 2026-07-20-R05 단순화 리팩터

### 변경

- 카메라별 얇은 모듈을 제거했다.
  - 제거: `line_scan_trigger.*`
  - 제거: `rgb_camera_trigger.*`
  - 제거: `trigger_board.*`
  - 제거: 빈 wrapper 구현이던 `src/trigger_output.cpp`
- `include/trigger_output.h`는 이제 header-only 재사용 API다.
  - namespace: `camera_trigger`
  - class: `camera_trigger::Output`
  - class: `camera_trigger::CameraTrigger`
- `CameraTrigger` 하나가 다음 동작을 공통 처리한다.
  - 주파수 설정
  - 속도 기반 주파수 계산
  - on/off
  - 주파수/주기 상태 조회
- 프로젝트별 카메라 핀, timer, channel, RGB/line-scan 설정은 `camera_trigger_settings.*`에만 둔다.
- 이전 `trigger_board` 통합부는 더 명확한 `camera_trigger_settings`로 이름을 변경했다.

### 현재 파일 구조

| 파일 | 역할 |
|---|---|
| `include/trigger_output.h` | 다른 프로젝트에서 재사용 가능한 header-only camera trigger API |
| `src/camera_trigger_settings.cpp` | 이 프로젝트의 line-scan/RGB 핀, timer, frequency 설정 |
| `include/camera_trigger_settings.h` | 이 프로젝트 trigger 설정 API |
| `src/esp32_ledc_pwm_output.cpp` | ESP32-C3 LEDC backend |
| `src/main.cpp` | USB CDC 수신과 camera trigger settings 호출 |

### 이식 방식

- 다른 프로젝트에서 ESP32가 아닌 보드를 사용하면 `camera_trigger::Output`을 구현하는 backend만 새로 만들면 된다.
- `camera_trigger::CameraTrigger`는 header-only로 그대로 재사용한다.
- 다른 카메라를 추가할 때도 카메라별 on/off/계산 cpp를 만들지 않고 `CameraTrigger` 인스턴스와 해당 설정만 추가한다.

### 검증

```text
2026-07-20: PlatformIO build SUCCESS
```
