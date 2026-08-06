#pragma once

#include <Arduino.h>
#include <driver/ledc.h>

#include "trigger_output.h"

struct Esp32LedcPwmConfig {
  gpio_num_t gpio;
  ledc_timer_t timer;
  ledc_channel_t channel;
  ledc_timer_bit_t resolution;
  uint32_t initialFrequencyHz;
};

class Esp32LedcPwmOutput : public camera_trigger::Output {
 public:
  bool begin(const Esp32LedcPwmConfig &config);
  bool setFrequencyHz(uint32_t frequencyHz) override;
  void stop() override;
  float frequencyHz() const override;
  float periodUs() const override;

 private:
  uint32_t halfDuty() const;

  Esp32LedcPwmConfig config_ = {};
  float actualFrequencyHz_ = 0.0f;
  bool begun_ = false;
};
