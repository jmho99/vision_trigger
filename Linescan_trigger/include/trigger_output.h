#pragma once

#include <math.h>
#include <stdint.h>

namespace camera_trigger {

class Output {
 public:
  virtual ~Output() = default;
  virtual bool setFrequencyHz(uint32_t frequencyHz) = 0;
  virtual void stop() = 0;
  virtual float frequencyHz() const = 0;
  virtual float periodUs() const = 0;
};

class CameraTrigger {
 public:
  explicit CameraTrigger(Output &output) : output_(output) {}

  bool setFrequencyHz(float frequencyHz) {
    if (!isfinite(frequencyHz) || frequencyHz <= 0.0f) {
      stop();
      return frequencyHz == 0.0f;
    }

    return output_.setFrequencyHz(static_cast<uint32_t>(lroundf(frequencyHz)));
  }

  bool setFrequencyFromSpeedKmh(
      float speedKmh,
      float distancePerTriggerMm,
      float maximumFrequencyHz) {
    if (!isfinite(speedKmh) || speedKmh <= 0.0f ||
        distancePerTriggerMm <= 0.0f) {
      stop();
      return speedKmh == 0.0f;
    }

    const float frequencyHz =
        speedKmh * (1000.0f / 3600.0f) *
        (1000.0f / distancePerTriggerMm);
    if (frequencyHz > maximumFrequencyHz) {
      return false;
    }

    return setFrequencyHz(frequencyHz);
  }

  void stop() {
    output_.stop();
  }

  float frequencyHz() const {
    return output_.frequencyHz();
  }

  float periodUs() const {
    return output_.periodUs();
  }

 private:
  Output &output_;
};

}  // namespace camera_trigger
