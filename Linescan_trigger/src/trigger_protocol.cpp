#include "trigger_protocol.h"

#include "config.h"

bool TriggerSerialPacketReader::push(
    uint8_t value,
    uint8_t (&packet)[TRIGGER_SERIAL_PACKET_SIZE]) {
  if (index_ == 0 && value != SERIAL_PACKET_HEADER) {
    return false;
  }

  buffer_[index_++] = value;
  if (index_ < TRIGGER_SERIAL_PACKET_SIZE) {
    return false;
  }

  for (uint8_t i = 0; i < TRIGGER_SERIAL_PACKET_SIZE; ++i) {
    packet[i] = buffer_[i];
  }
  reset();
  return true;
}

void TriggerSerialPacketReader::reset() {
  index_ = 0;
}

bool parseTriggerCommandPacket(
    const uint8_t (&packet)[TRIGGER_SERIAL_PACKET_SIZE],
    TriggerCommand &command,
    TriggerProtocolError &error) {
  const uint8_t header = packet[0];
  const uint8_t flag = packet[1];
  const uint8_t speedKmh = packet[2];
  const uint8_t checksum = packet[3];

  error = TriggerProtocolError::None;

  if ((header ^ flag ^ speedKmh) != checksum) {
    error = TriggerProtocolError::Checksum;
    return false;
  }

  if (flag != SERIAL_FLAG_TRIG_ON_STAT_OFF &&
      flag != SERIAL_FLAG_TRIG_OFF_STAT_OFF &&
      flag != SERIAL_FLAG_TRIG_ON_STAT_ON &&
      flag != SERIAL_FLAG_TRIG_OFF_STAT_ON) {
    error = TriggerProtocolError::Flag;
    return false;
  }

  command.triggerEnabled =
      flag == SERIAL_FLAG_TRIG_ON_STAT_OFF ||
      flag == SERIAL_FLAG_TRIG_ON_STAT_ON;
  command.statusRequested =
      flag == SERIAL_FLAG_TRIG_ON_STAT_ON ||
      flag == SERIAL_FLAG_TRIG_OFF_STAT_ON;
  command.speedKmh = speedKmh;
  return true;
}
