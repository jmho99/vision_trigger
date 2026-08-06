# Trigger Board Crosscheck - 2026-07-20

이 문서는 `docs/CODEX_TRIGGER_BOARD_HANDOFF.md`와 `docs/GPT_TRIGGER_BOARD_MASTER_HANDOFF.md`를 비교해, 서로 다른 내용/추가 반영 필요 항목/충돌 가능성을 정리한 문서입니다.

## 기준 문서

| 구분 | 파일 | 역할 |
|---|---|---|
| Codex 대화 기준 | `docs/CODEX_TRIGGER_BOARD_HANDOFF.md` | 현재 로컬 코드와 Codex 대화에서 확정/구현된 내용 |
| Web GPT 대화 기준 | `docs/GPT_TRIGGER_BOARD_MASTER_HANDOFF.md` | 웹 GPT 대화에서 정리된 요구사항/운영 방향/변경 이력 |

## 크로스체크 결과 요약

### 일치하는 내용

- 보드는 Seeed Studio XIAO ESP32-C3를 사용합니다.
- MCU는 ESP32-C3이며, PlatformIO 빌드 기준 160 MHz입니다.
- Jetson/PC와 보드는 USB CDC Serial 방식으로 통신합니다.
- 현재 baud rate는 115200 bps입니다.
- 현재 기본 패킷은 4-byte binary frame입니다.

```text
[0xAA][flag][speed_kmh][checksum]
```

- `speed_kmh`는 현재 `uint8_t`, 단위는 정수 km/h입니다.
- 기본 전송 주기는 Python 운영 스크립트 기준 20 Hz입니다.
- 라인스캔 트리거 계산식은 동일합니다.

```text
trigger_hz = speed_km_h / 3.6 * (1000 / distance_per_trigger_mm)
```

- 현재 펌웨어는 ESP32-C3 LEDC PWM으로 D2/GPIO4에 라인스캔 트리거를 출력합니다.
- 현재 구현은 50% duty PWM이며, 고정 pulse width 방식은 아직 미구현입니다.
- 차량 속도 0 또는 trigger OFF 요청 시 라인스캔 trigger 출력은 정지됩니다.
- USB/UART 통신 단절 시 펌웨어 자동 정지 timeout은 아직 미구현입니다.

## 차이 또는 충돌 가능 항목

### 1. 라인스캔 최대 주파수

#### Codex 문서/현재 코드

현재 코드의 제한값:

```cpp
constexpr float MAX_CAMERA_LINE_RATE_HZ = 60000.0f;
```

즉 현재 펌웨어 보호 한계는 60 kHz입니다.

관련 파일:

- `include/config.h`
- `src/trigger_output.cpp`
  - `setTriggerFromSpeed()`

#### GPT 문서

GPT 문서에는 실제 운용 요구 최대값을 56 kHz로 제한하는 방향이 기록되어 있습니다.

#### 판단

현재 코드와 최신 요구사항 사이에 차이가 있습니다.

#### 반영 필요

- 실제 운용 최대 line trigger를 56 kHz로 확정할지 확인 필요.
- 확정되면 `MAX_CAMERA_LINE_RATE_HZ`를 `56000.0f`로 변경해야 합니다.
- `uint8_t speed_kmh` 구조에서는 1 mm/trigger 기준 201 km/h가 약 55.833 kHz, 202 km/h가 약 56.111 kHz입니다. 56 kHz를 엄격히 지키려면 202 km/h 이상은 reject 또는 clamp가 필요합니다.

### 2. RGB 카메라 트리거

#### Codex 문서/현재 코드

RGB 카메라 trigger는 미구현으로 기록되어 있습니다.

현재 코드에는 RGB 카메라용 GPIO, timer/channel, 함수가 없습니다.

#### GPT 문서

GPT 문서에는 RGB 카메라 목표 trigger가 20 Hz, 주기 50 ms로 기록되어 있습니다.

#### 판단

요구사항은 추가되었지만 코드에는 아직 반영되지 않았습니다.

#### 반영 필요

- RGB 카메라 trigger 출력 핀 결정.
- RGB 카메라 trigger 입력 전압/극성/pulse width 확인.
- 라인스캔 trigger와 독립 timer를 사용할지, 동일 상태 명령에서 같이 제어할지 결정.

### 3. 프로젝트 범위

#### Codex 문서

Jetson 속도 입력 소스는 아직 미정으로 기록되어 있습니다.

#### GPT 문서

Jetson 내부 GPS/OBD 수집 및 통합 로직은 trigger board 구현 범위에서 제외한다고 정리되어 있습니다.

#### 판단

서로 충돌하지는 않지만, GPT 문서가 범위를 더 명확히 제한합니다.

#### 반영 필요

Codex 기준 문서에도 아래 원칙을 명확히 추가하는 것이 좋습니다.

```text
Trigger board는 Jetson에서 이미 계산/선택된 최종 속도값과 trigger 명령만 받는다.
GPS/OBD 수집, 필터링, 속도 융합 로직은 trigger board 펌웨어 범위가 아니다.
```

### 4. 출력 파형

#### Codex 문서/현재 코드

현재 구현은 50% duty PWM입니다.

100 km/h, 1 mm/trigger 기준:

```text
frequency ≈ 27,778 Hz
period ≈ 36 us
high ≈ 18 us
low ≈ 18 us
```

#### GPT 문서

고정 HIGH pulse width 방식 검토가 필요하다고 기록되어 있습니다.

#### 판단

현재 구현과 검토 방향은 다릅니다. 아직 요구사항 확정은 아닙니다.

#### 반영 필요

- 카메라의 최소 HIGH/LOW time 확인 후 결정.
- 50% duty PWM 유지 또는 고정 pulse width 방식으로 변경 여부 결정.

### 5. UART/USB 통신 단절 안전 동작

#### Codex 문서/현재 코드

펌웨어 timeout은 미구현입니다.

현재는 마지막으로 받은 trigger ON + speed 값이 유지되면 USB 통신이 끊겨도 PWM이 계속 출력될 수 있습니다.

#### GPT 문서

USB packet timeout 구현이 권장 우선순위에 포함되어 있습니다.

#### 판단

구현 우선순위로 올라온 미구현 항목입니다.

#### 반영 필요

펌웨어에 아래 방식 추가 검토:

```text
마지막 정상 packet 수신 시각 저장
SERIAL_TIMEOUT_MS 동안 정상 packet이 없으면 stopTrigger()
```

관련 후보:

- `src/main.cpp`
  - `handleSerialPacket()`
  - `loop()`

### 6. 문서 파일명/관리 방식

#### 현재 docs 상태

현재 존재 파일:

- `docs/CODEX_TRIGGER_BOARD_HANDOFF.md`
- `docs/GPT_TRIGGER_BOARD_MASTER_HANDOFF.md`
- `docs/CROSSCHECK_TRIGGER_BOARD_2026-07-20.md`

#### GPT 문서의 권장

GPT 문서에는 최신 통합 문서명으로 `TRIGGER_BOARD_MASTER_HANDOFF.md` 및 날짜별 history snapshot을 관리하는 방향이 언급되어 있습니다.

#### 판단

사용자가 현재 `CODEX_...`와 `GPT_...`로 출처를 나누겠다고 했으므로, 지금은 출처별 문서 유지가 우선입니다.

#### 반영 필요

향후 필요하면 아래 구조로 정리할 수 있습니다.

```text
docs/CODEX_TRIGGER_BOARD_HANDOFF.md
docs/GPT_TRIGGER_BOARD_MASTER_HANDOFF.md
docs/CROSSCHECK_TRIGGER_BOARD_YYYY-MM-DD.md
docs/TRIGGER_BOARD_MASTER_HANDOFF.md        # 선택: 통합본
docs/history/...                            # 선택: 스냅샷
```

## Codex 기준 문서에 반영 권장 항목

아래 항목은 `CODEX_TRIGGER_BOARD_HANDOFF.md`에 추가 반영하는 것이 좋습니다.

### 추가 권장

- GPT 문서와 크로스체크해야 한다는 관리 규칙.
- Jetson 내부 속도 취득/융합 로직은 trigger board 범위 밖이라는 범위 정의.
- RGB 카메라 trigger 요구 후보:
  - 20 Hz
  - 50 ms period
  - 미구현
  - GPIO/전압/극성/pulse width 확인 필요
- 라인스캔 운용 최대 주파수 후보:
  - 현재 코드: 60 kHz
  - 요구 후보: 56 kHz
  - 확정 전까지 충돌 가능 항목으로 유지

### 변경 검토 권장

- `MAX_CAMERA_LINE_RATE_HZ`를 60 kHz에서 56 kHz로 변경할지 검토.
- USB/CDC packet timeout 구현 여부 검토.
- line-scan trigger를 50% duty PWM에서 고정 pulse width 방식으로 바꿀지 검토.

## 현재 코드와 GPT 문서 사이의 구현 갭

| 항목 | GPT 문서/요구 | 현재 코드 | 상태 |
|---|---|---|---|
| RGB trigger | 20 Hz 후보 | 없음 | 미구현 |
| line-scan max | 56 kHz 후보 | 60 kHz limit | 불일치/확인 필요 |
| USB timeout | 구현 권장 | 없음 | 미구현 |
| 고정 pulse width | 검토 | 50% duty PWM | 미확정 |
| Jetson 속도 source | trigger board 범위 제외 | Python 콘솔 입력 stub | 실제 연동 미구현 |

## 2026-07-20 변경 이력

### 추가

- `CODEX_TRIGGER_BOARD_HANDOFF.md`와 `GPT_TRIGGER_BOARD_MASTER_HANDOFF.md`의 크로스체크 문서를 추가했습니다.
- RGB trigger 20 Hz 후보, line-scan 56 kHz 후보, USB timeout 미구현 상태를 비교 항목으로 정리했습니다.

### 변경

- 코드 변경 없음.
- 기존 handoff 문서 본문 변경 없음.

### 삭제

- 없음.

### 미결정

- 56 kHz를 코드 제한값으로 확정할지 여부.
- RGB trigger 구현 여부 및 핀/타이머.
- USB 통신 timeout 값.
- 50% duty PWM 유지 여부.

## 2026-07-20-R02 추가 크로스체크

### 반영된 사용자 결정

- 라인스캔 최대 보호 주파수는 `60000.0f`로 유지한다.
  - 이전 GPT 문서의 56 kHz 후보는 현재 결정 기준으로 대체됨.
  - 현재 코드 변경은 필요 없음.
- Jetson은 보정된 최종 속도값을 전달한다.
  - GPS/OBD/센서 융합 로직은 트리거 보드 범위 밖으로 확정.
- RGB 카메라 트리거도 트리거 보드에서 생성하는 방향으로 요구사항이 확장됨.
  - 목적: 라인스캔 카메라와 동일 장면을 함께 촬영.

### 정정 또는 기술 주의

- RGB와 라인스캔을 “같이 촬영”하는 것과 “동일 timer/동일 주파수로 구동”하는 것은 다르다.
- 라인스캔은 속도 기반 kHz~수십 kHz trigger가 필요하다.
- RGB가 일반 area-scan 카메라라면 보통 20 Hz 등 저주파 trigger가 필요할 가능성이 높다.
- 두 카메라의 trigger 주파수가 다르면 동일 LEDC timer를 공유할 수 없다.
- 같은 timer를 공유하려면 두 출력이 같은 주파수와 같은 timer 설정을 사용해야 한다.

### LEDC 관련 정정

- 현재 구현의 LEDC는 고정 pulse width 출력이 아니라 50% duty PWM 출력이다.
- 관련 코드:
  - `src/trigger_output.cpp`
  - `setTriggerFromSpeed()`
  - `HALF_DUTY = 512`
  - `LEDC_RESOLUTION = LEDC_TIMER_10_BIT`
- 고정 pulse width가 필요하면 아래 중 하나를 검토해야 한다.
  - 주파수 변경 시 LEDC duty를 pulse width에 맞게 재계산
  - ESP32 timer interrupt/GPTimer/RMT 기반 pulse generator로 변경

### 업데이트된 구현 갭

| 항목 | 현재 결정/요구 | 현재 코드 | 상태 |
|---|---|---|---|
| line-scan max | 60 kHz 유지 | `60000.0f` | 일치 |
| Jetson speed | 보정된 최종 속도값 전달 | `uint8_t speed_kmh` 수신 | 인터페이스 일치, 실제 Jetson 연동 미구현 |
| RGB trigger | 보드에서 생성 방향 | 없음 | 미구현 |
| RGB/line 동기화 | 동일 장면 촬영 | 없음 | 방식 미정 |
| same timer 사용 | 가능성 검토 | line-scan LEDC timer0 사용 중 | 주파수 동일 여부 확인 필요 |
| fixed pulse width | 필요 여부 재확인 | 50% duty PWM | 미구현/정정 필요 |

### 추가 확인 필요

- RGB 카메라 모델명.
- RGB trigger 목표 주파수.
- RGB trigger pulse width, polarity, input voltage.
- RGB와 line-scan trigger가 같은 입력 조건인지.
- RGB를 line trigger와 완전히 같은 pulse로 받을 수 있는지.
- RGB가 별도 저주파 frame trigger를 요구하는지.

## 2026-07-20-R03 추가 크로스체크

### 반영된 사용자 결정

- RGB 카메라는 `AcquisitionStart` trigger로 동작시키는 방향이다.
- 라인스캔 카메라는 `LineStart` trigger로 동작시키는 방향이다.
- RGB와 라인스캔은 trigger Hz가 다르므로 별도 timer/channel로 동작시킨다.
- 현재 확인 기준으로 pulse width는 큰 문제가 아니므로 당장 고정 pulse width 방식으로 변경하지 않는다.

### 해소된 항목

- “RGB와 라인스캔이 동일 timer를 쓸 수 있는가?”에 대한 방향:
  - 두 카메라의 Hz가 다르므로 동일 timer 사용은 부적합.
  - 별도 timer/channel 사용으로 정리.
- “고정 pulse width가 필요한가?”에 대한 방향:
  - 현재는 필요성이 낮음.
  - 기존 50% duty PWM 유지 가능.
  - 단, 카메라 입력 조건 문제가 발견되면 재검토.

### 업데이트된 구현 갭

| 항목 | 현재 결정/요구 | 현재 코드 | 상태 |
|---|---|---|---|
| line-scan trigger selector | `LineStart` | GPIO4 LEDC 출력만 구현 | 카메라 설정 필요 |
| RGB trigger selector | `AcquisitionStart` | 없음 | 미구현 |
| line/RGB timer | 별도 timer/channel | line-scan만 LEDC timer0/channel0 | RGB timer 미구현 |
| pulse width | 현재 민감하지 않음 | 50% duty PWM | 유지 가능 |

### 추가 확인 필요

- RGB trigger 목표 Hz.
- RGB trigger 출력 GPIO.
- RGB용 LEDC timer/channel 배정.
- RGB가 `AcquisitionStart` 1회 후 continuous acquisition인지, 일정 주기마다 `AcquisitionStart`가 필요한지.
- 라인스캔 카메라 설정에서 `LineStart` trigger가 정확히 적용되어 있는지.

## 2026-07-20-R04 코드 반영 크로스체크

### 반영된 코드 변경

- line-scan과 RGB trigger가 별도 모듈로 분리됨.
- ESP32-C3 LEDC backend가 `esp32_ledc_pwm_output.*`로 분리됨.
- packet parser가 `trigger_protocol.*`로 분리됨.
- 전체 trigger 상태 관리는 `trigger_board.*`로 이동됨.
- fixed arena allocator가 추가되고 `TriggerSerialPacketReader` 생성에 사용됨.

### 현재 구현 상태

| 항목 | 결정/요구 | 현재 코드 | 상태 |
|---|---|---|---|
| line-scan trigger | `LineStart`, 속도 기반 | `line_scan_trigger.*` | 구현 |
| RGB trigger | `AcquisitionStart`, 별도 timer | `rgb_camera_trigger.*` | 1차 구현 |
| line/RGB timer 분리 | 필요 | timer0/channel0, timer1/channel1 | 구현 |
| ESP32 backend 분리 | 필요 | `esp32_ledc_pwm_output.*` | 구현 |
| protocol 모듈화 | 필요 | `trigger_protocol.*` | 구현 |
| allocator | heap 단편화 최소화 | `fixed_arena.h` | 1차 적용 |
| cross-platform 입력 소스 고려 | Ouster/PPS 등 | `TriggerInputKind` enum | 인터페이스만 준비 |

### 현재 핀/타이머

| 카메라 | Pin | GPIO | Timer | Channel |
|---|---:|---:|---|---|
| line-scan | D2 | GPIO4 | `LEDC_TIMER_0` | `LEDC_CHANNEL_0` |
| RGB | D3 | GPIO5 | `LEDC_TIMER_1` | `LEDC_CHANNEL_1` |

### 추가 확인 필요

- RGB D3/GPIO5 배선 확정.
- RGB `AcquisitionStart`가 주기적 20 Hz pulse를 받는 형태가 맞는지.
- RGB가 실제로는 1회 AcquisitionStart 후 내부 frame rate로 촬영하는지.
- Jetson/Raspberry Pi/Arduino용 backend를 실제로 분리할 필요가 생기는 시점.
## 2026-07-20-R05 단순화 반영

### 코드 구조 변경

- 기존 카메라별 모듈과 `trigger_board` 중간 계층은 제거하고, 단일 재사용 API로 통합했다.
- `camera_trigger::CameraTrigger`가 line-scan/RGB 공통 on/off/주파수 계산을 처리한다.
- 프로젝트 고유 설정은 `camera_trigger_settings.*`에만 남긴다.
- ESP32 특수 기능은 `Esp32LedcPwmOutput` backend에만 남긴다.

### 상태

| 항목 | 현재 구조 |
|---|---|
| 범용 camera trigger | `include/trigger_output.h` header-only |
| 프로젝트 설정 | `camera_trigger_settings.*` |
| ESP32 LEDC | `esp32_ledc_pwm_output.*` |
| 빈 wrapper cpp | 제거 |
| 카메라별 얇은 cpp | 제거 |
