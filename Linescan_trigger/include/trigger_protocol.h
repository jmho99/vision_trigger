#pragma once

#include <Arduino.h>

#include "trigger_types.h"

constexpr uint8_t TRIGGER_SERIAL_PACKET_SIZE = 4;

enum class TriggerProtocolError : uint8_t {
  None,
  Checksum,
  Flag,
};

class TriggerSerialPacketReader {
 public:
  bool push(uint8_t value, uint8_t (&packet)[TRIGGER_SERIAL_PACKET_SIZE]);
  void reset();

 private:
  uint8_t buffer_[TRIGGER_SERIAL_PACKET_SIZE] = {};
  uint8_t index_ = 0;
};

bool parseTriggerCommandPacket(
    const uint8_t (&packet)[TRIGGER_SERIAL_PACKET_SIZE],
    TriggerCommand &command,
    TriggerProtocolError &error);
