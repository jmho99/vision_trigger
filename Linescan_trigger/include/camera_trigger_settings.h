#pragma once

#include <Arduino.h>

#include "trigger_types.h"

struct CameraTriggerSettingsStatus {
  bool requestedTriggerEnabled = false;
  uint8_t speedKmh = 0;
  float lineScanHz = 0.0f;
  float lineScanPeriodUs = 0.0f;
  float rgbCameraHz = 0.0f;
  float rgbCameraPeriodUs = 0.0f;
};

bool beginCameraTriggerSettings();
bool applyCameraTriggerSettings(const TriggerCommand &command, bool &changed);
void stopCameraTriggerSettings();
CameraTriggerSettingsStatus cameraTriggerSettingsStatus();
