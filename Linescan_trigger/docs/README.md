# XIAO ESP32-C3 line-scan trigger board

차량 속도에 맞춰 line-scan camera trigger 신호를 만드는 PlatformIO 프로젝트입니다.

현재 기본 입력은 USB Serial binary packet입니다. CAN/TWAI 코드는 남겨두었고,
`include/config.h`의 `ENABLE_CAN_SPEED_INPUT`을 `true`로 바꾸면 기존 CAN 입력 경로를 다시 사용할 수 있습니다.

## Serial binary packet

PC에서 보드로 4 byte 고정 패킷을 보냅니다.

| Byte | Field | Type | Description |
|---:|---|---|---|
| 0 | Header | `uint8_t` | 항상 `0xAA` |
| 1 | Flag | `uint8_t` | trigger/status 동작 선택 |
| 2 | Speed | `uint8_t` | 차량 속도, `0~255 km/h` |
| 3 | Checksum | `uint8_t` | `header ^ flag ^ speed` |

Flag:

| Value | Meaning |
|---:|---|
| `1` | trigger on, status off |
| `2` | trigger off, status off |
| `3` | trigger on, status on |
| `4` | trigger off, status on |

예시:

| Meaning | Bytes |
|---|---|
| 100 km/h, trigger on, no status | `AA 01 64 CF` |
| 100 km/h, trigger on, status print | `AA 03 64 CD` |
| trigger off, no status | `AA 02 00 A8` |
| trigger off, status print | `AA 04 00 AE` |

Status request가 켜져 있으면 보드는 사람이 읽을 수 있는 문자열을 출력합니다.

```text
OK trig=1 speed=100 km/h line_hz=27778 line_us=36.000 rgb_hz=20 rgb_us=50000.000
```

## Python send example

```python
import serial

ser = serial.Serial("COM7", 115200, timeout=1)

HEADER = 0xAA
TRIG_ON_STAT_ON = 3

flag = TRIG_ON_STAT_ON
speed = 100
checksum = HEADER ^ flag ^ speed

ser.write(bytes([HEADER, flag, speed, checksum]))
print(ser.readline().decode(errors="replace").strip())
```

또는 포함된 송수신 스크립트를 사용할 수 있습니다.

```powershell
python tools\send_serial_packet.py COM7 100 --trigger --status
python tools\send_serial_packet.py COM7 0 --no-trigger --status
```

운영/연속 전송 테스트는 아래 스크립트를 사용합니다.

```powershell
python tools\stream_serial_speed.py COM7 --speed 100 --rate 20 --trigger
```

기본적으로 COM 포트를 계속 열어두고 초당 20회 속도 패킷을 보냅니다.
실행 중 콘솔에 새 속도를 입력하면 다음 패킷부터 반영됩니다.

```text
> 90
> 100
> off
> on
> status
> quit
```

1초마다 status를 요청해 보드 응답을 출력하고, `Ctrl+C` 또는 `quit`으로 종료하면
trigger off 패킷을 보낸 뒤 종료합니다.

카메라 acquisition을 계속 유지한 채 Python만 종료하고 싶으면 trigger off 패킷을 보내지 않도록
아래 옵션을 사용합니다.

```powershell
python tools\stream_serial_speed.py COM7 --speed 100 --rate 20 --trigger --no-stop-on-exit
```

스크립트는 serial port를 열 때 DTR/RTS를 낮추고, 기본 1.2초 대기 후 첫 패킷을 보냅니다.
보드 리셋 타이밍 때문에 첫 패킷이 씹히는 경우 `--open-delay 2.0`처럼 값을 늘려보세요.

## Trigger calculation

기본값은 1 mm 이동마다 trigger 1회입니다.

```text
trigger_hz = speed_km_h / 3.6 * (1000 / distance_per_trigger_mm)
```

100 km/h, 1 mm/trigger 기준:

```text
trigger_hz = 27777.78 Hz
period = 36 us
```

## Pin map

| Function | XIAO pin | ESP32-C3 GPIO |
|---|---:|---:|
| Line-scan camera trigger output | D2 | GPIO4 |
| RGB camera trigger output | D3 | GPIO5 |
| TWAI TX -> CAN transceiver D | D4 | GPIO6 |
| TWAI RX <- CAN transceiver R | D5 | GPIO7 |

XIAO 출력은 3.3 V입니다. 카메라 입력 전압, 입력 방식, 최소 pulse width를 확인하고
필요하면 라인 드라이버/포토커플러/레벨 변환 회로를 사용하세요.

## Source structure

| File | Role |
|---|---|
| `src/main.cpp` | USB CDC packet 수신, 상태 출력, 전체 trigger 설정 적용 |
| `src/camera_trigger_settings.cpp` | RGB/line-scan 설정과 동시 start/stop 관리 |
| `include/trigger_output.h` | 재사용 가능한 header-only `camera_trigger::CameraTrigger` API |
| `src/esp32_ledc_pwm_output.cpp` | ESP32-C3 LEDC board-specific PWM backend |
| `src/trigger_protocol.cpp` | 4-byte binary packet parser |
| `include/fixed_arena.h` | heap 단편화를 피하기 위한 고정 arena allocator |

현재 line-scan과 RGB는 서로 다른 Hz를 사용하므로 별도 LEDC timer/channel로 동작합니다.
line-scan은 LEDC timer0/channel0, RGB는 LEDC timer1/channel1을 사용합니다.
