#include "can_speed.h"

#include <driver/twai.h>

#include "config.h"

namespace {
bool canStarted = false;
bool speedAvailable = false;
float latestSpeedKmh = 0.0f;
uint32_t lastRequestMs = 0;

void requestVehicleSpeed() {
  // Functional OBD-II request: Mode 01, PID 0x0D (vehicle speed).
  twai_message_t request = {};
  request.identifier = 0x7DF;
  request.data_length_code = 8;
  request.data[0] = 0x02;
  request.data[1] = 0x01;
  request.data[2] = 0x0D;
  request.data[3] = 0x55;
  request.data[4] = 0x55;
  request.data[5] = 0x55;
  request.data[6] = 0x55;
  request.data[7] = 0x55;
  twai_transmit(&request, 0);
}

bool parseVehicleSpeed(const twai_message_t &message, float &speedKmh) {
  const bool isEcuResponse =
      message.identifier >= 0x7E8 && message.identifier <= 0x7EF;
  if (!isEcuResponse || message.data_length_code < 4) {
    return false;
  }

  if (message.data[1] != 0x41 || message.data[2] != 0x0D) {
    return false;
  }

  speedKmh = static_cast<float>(message.data[3]);
  return true;
}
}  // namespace

bool beginCanSpeed() {
  if (!ENABLE_CAN_SPEED_INPUT) {
    return true;
  }

  const twai_general_config_t general =
      TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_GPIO, CAN_RX_GPIO, TWAI_MODE_NORMAL);
  const twai_timing_config_t timing = TWAI_TIMING_CONFIG_500KBITS();
  const twai_filter_config_t filter = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  if (twai_driver_install(&general, &timing, &filter) != ESP_OK) {
    return false;
  }
  if (twai_start() != ESP_OK) {
    twai_driver_uninstall();
    return false;
  }

  canStarted = true;
  return true;
}

void updateCanSpeed() {
  if (!canStarted) {
    return;
  }

  const uint32_t now = millis();
  if (now - lastRequestMs >= CAN_SPEED_REQUEST_INTERVAL_MS) {
    lastRequestMs = now;
    requestVehicleSpeed();
  }

  twai_message_t message = {};
  while (twai_receive(&message, 0) == ESP_OK) {
    float parsedSpeedKmh = 0.0f;
    if (parseVehicleSpeed(message, parsedSpeedKmh)) {
      latestSpeedKmh = parsedSpeedKmh;
      speedAvailable = true;
    }
  }
}

bool takeCanSpeed(float &speedKmh) {
  if (!speedAvailable) {
    return false;
  }

  speedKmh = latestSpeedKmh;
  speedAvailable = false;
  return true;
}

