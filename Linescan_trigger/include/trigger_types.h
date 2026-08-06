#pragma once

#include <stdint.h>

enum class TriggerInputKind : uint8_t {
  SpeedKmh,
  OusterLidarAngle,
  Pps,
  ExternalPulse,
};

struct TriggerInputSample {
  TriggerInputKind kind = TriggerInputKind::SpeedKmh;
  float value = 0.0f;
};

struct TriggerCommand {
  bool triggerEnabled = false;
  bool statusRequested = false;
  uint8_t speedKmh = 0;
};
