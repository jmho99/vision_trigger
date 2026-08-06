#include "camera_trigger_settings.h"

#include "config.h"
#include "esp32_ledc_pwm_output.h"
#include "trigger_output.h"

namespace {
Esp32LedcPwmOutput lineScanOutput;
Esp32LedcPwmOutput rgbCameraOutput;
camera_trigger::CameraTrigger lineScanTrigger(lineScanOutput);
camera_trigger::CameraTrigger rgbCameraTrigger(rgbCameraOutput);

bool requestedTriggerEnabled = false;
uint8_t latestSpeedKmh = 0;
}  // namespace

bool beginCameraTriggerSettings() {
  const Esp32LedcPwmConfig lineScanConfig = {
      .gpio = LINE_SCAN_TRIGGER_GPIO,
      .timer = LEDC_TIMER_0,
      .channel = LEDC_CHANNEL_0,
      .resolution = LEDC_TIMER_10_BIT,
      .initialFrequencyHz = 1000,
  };
  const Esp32LedcPwmConfig rgbCameraConfig = {
      .gpio = RGB_CAMERA_TRIGGER_GPIO,
      .timer = LEDC_TIMER_1,
      .channel = LEDC_CHANNEL_1,
      .resolution = LEDC_TIMER_14_BIT,
      .initialFrequencyHz = 20,
  };

  return lineScanOutput.begin(lineScanConfig) && rgbCameraOutput.begin(rgbCameraConfig);
}

bool applyCameraTriggerSettings(const TriggerCommand &command, bool &changed) {
  changed =
      command.triggerEnabled != requestedTriggerEnabled ||
      command.speedKmh != latestSpeedKmh;
  if (!changed) {
    return true;
  }

  requestedTriggerEnabled = command.triggerEnabled;
  latestSpeedKmh = command.speedKmh;

  if (!requestedTriggerEnabled || latestSpeedKmh == 0) {
    stopCameraTriggerSettings();
    return true;
  }

  const bool lineScanOk = lineScanTrigger.setFrequencyFromSpeedKmh(
      static_cast<float>(latestSpeedKmh),
      DISTANCE_PER_TRIGGER_MM,
      MAX_CAMERA_LINE_RATE_HZ);
  const bool rgbCameraOk = rgbCameraTrigger.setFrequencyHz(RGB_CAMERA_TRIGGER_HZ);
  if (!lineScanOk || !rgbCameraOk) {
    stopCameraTriggerSettings();
    return false;
  }

  return true;
}

void stopCameraTriggerSettings() {
  lineScanTrigger.stop();
  rgbCameraTrigger.stop();
}

CameraTriggerSettingsStatus cameraTriggerSettingsStatus() {
  CameraTriggerSettingsStatus status = {};
  status.requestedTriggerEnabled = requestedTriggerEnabled;
  status.speedKmh = latestSpeedKmh;
  status.lineScanHz = lineScanTrigger.frequencyHz();
  status.lineScanPeriodUs = lineScanTrigger.periodUs();
  status.rgbCameraHz = rgbCameraTrigger.frequencyHz();
  status.rgbCameraPeriodUs = rgbCameraTrigger.periodUs();
  return status;
}
