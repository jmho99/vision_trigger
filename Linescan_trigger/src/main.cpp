#include <Arduino.h>
#include <new>

#include "can_speed.h"
#include "camera_trigger_settings.h"
#include "config.h"
#include "fixed_arena.h"
#include "trigger_protocol.h"

namespace {
FixedArena<128> runtimeArena;
TriggerSerialPacketReader *serialPacketReader = nullptr;
uint32_t lastCanSpeedMs = 0;

void printTriggerBoardStatus(const char *prefix) {
  const CameraTriggerSettingsStatus status = cameraTriggerSettingsStatus();
  Serial.printf(
      "%s trig=%u speed=%u km/h line_hz=%.0f line_us=%.3f rgb_hz=%.0f rgb_us=%.3f\n",
      prefix,
      status.requestedTriggerEnabled ? 1 : 0,
      status.speedKmh,
      status.lineScanHz,
      status.lineScanPeriodUs,
      status.rgbCameraHz,
      status.rgbCameraPeriodUs);
}

void handleSerialCommand(const TriggerCommand &command) {
  bool changed = false;
  const bool ok = applyCameraTriggerSettings(command, changed);

  if (command.statusRequested) {
    printTriggerBoardStatus(ok ? "OK" : "ERR output");
  }
}

void handleSerialPacketError(TriggerProtocolError error) {
  switch (error) {
    case TriggerProtocolError::Checksum:
      Serial.println("ERR checksum");
      break;
    case TriggerProtocolError::Flag:
      Serial.println("ERR flags");
      break;
    case TriggerProtocolError::None:
    default:
      break;
  }
}

void updateSerialInput() {
  while (Serial.available() > 0) {
    uint8_t packet[TRIGGER_SERIAL_PACKET_SIZE] = {};
    const uint8_t value = static_cast<uint8_t>(Serial.read());
    if (serialPacketReader == nullptr || !serialPacketReader->push(value, packet)) {
      continue;
    }

    TriggerCommand command = {};
    TriggerProtocolError error = TriggerProtocolError::None;
    if (!parseTriggerCommandPacket(packet, command, error)) {
      handleSerialPacketError(error);
      continue;
    }

    handleSerialCommand(command);
  }
}

void applyCanSpeed(float speedKmh) {
  if (speedKmh < 0.0f || speedKmh > 255.0f) {
    Serial.printf("ERROR: %.2f km/h is invalid or exceeds the %.0f Hz camera limit.\n",
                  speedKmh, MAX_CAMERA_LINE_RATE_HZ);
    return;
  }

  TriggerCommand command = {};
  command.triggerEnabled = true;
  command.speedKmh = static_cast<uint8_t>(speedKmh);
  bool changed = false;
  if (!applyCameraTriggerSettings(command, changed)) {
    Serial.printf("ERROR: %.2f km/h is invalid or exceeds the %.0f Hz camera limit.\n",
                  speedKmh, MAX_CAMERA_LINE_RATE_HZ);
    return;
  }

  const CameraTriggerSettingsStatus status = cameraTriggerSettingsStatus();

  Serial.printf("[CAN] speed=%.2f km/h, line_hz=%.0f, line_us=%.3f\n",
                speedKmh,
                status.lineScanHz,
                status.lineScanPeriodUs);
}
}  // namespace

void setup() {
  runtimeArena.reset();

  Serial.begin(SERIAL_BAUD_RATE);
  delay(1000);

  void *readerStorage = runtimeArena.allocate(
      sizeof(TriggerSerialPacketReader),
      alignof(TriggerSerialPacketReader));
  if (readerStorage == nullptr) {
    Serial.println("ERROR: runtime arena allocation failed.");
    return;
  }
  serialPacketReader = new (readerStorage) TriggerSerialPacketReader();

  if (!beginCameraTriggerSettings()) {
    Serial.println("ERROR: trigger board output initialization failed.");
    return;
  }
  if (!beginCanSpeed()) {
    Serial.println("ERROR: TWAI/CAN initialization failed.");
    return;
  }

  Serial.println("\nXIAO ESP32-C3 camera trigger board");
  Serial.println("Serial packet: [0xAA][flag][speed_kmh][checksum]");
  Serial.println("flag: 1=trig on, 2=trig off, 3=trig on+status, 4=trig off+status");
  Serial.println("checksum=header^flag^speed");
  Serial.println("line-scan: LineStart, RGB: AcquisitionStart");
  Serial.printf("Mode: %s\n",
                ENABLE_CAN_SPEED_INPUT ? "CAN OBD-II PID 0x0D" : "USB serial binary");
}

void loop() {
  updateSerialInput();

  if (ENABLE_CAN_SPEED_INPUT) {
    updateCanSpeed();

    float speedKmh = 0.0f;
    if (takeCanSpeed(speedKmh)) {
      lastCanSpeedMs = millis();
      applyCanSpeed(speedKmh);
    }

    if (cameraTriggerSettingsStatus().lineScanHz > 0.0f &&
        millis() - lastCanSpeedMs > CAN_SPEED_TIMEOUT_MS) {
      stopCameraTriggerSettings();
      Serial.println("CAN speed timeout: trigger board stopped.");
    }
  }
}
