#include "esp32_ledc_pwm_output.h"

bool Esp32LedcPwmOutput::begin(const Esp32LedcPwmConfig &config) {
  config_ = config;

  const ledc_timer_config_t timer = {
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .duty_resolution = config_.resolution,
      .timer_num = config_.timer,
      .freq_hz = config_.initialFrequencyHz,
      .clk_cfg = LEDC_AUTO_CLK,
  };
  if (ledc_timer_config(&timer) != ESP_OK) {
    return false;
  }

  const ledc_channel_config_t channel = {
      .gpio_num = static_cast<int>(config_.gpio),
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .channel = config_.channel,
      .intr_type = LEDC_INTR_DISABLE,
      .timer_sel = config_.timer,
      .duty = 0,
      .hpoint = 0,
  };
  if (ledc_channel_config(&channel) != ESP_OK) {
    return false;
  }

  begun_ = true;
  actualFrequencyHz_ = 0.0f;
  return true;
}

bool Esp32LedcPwmOutput::setFrequencyHz(uint32_t frequencyHz) {
  if (!begun_ || frequencyHz == 0) {
    stop();
    return frequencyHz == 0;
  }

  if (ledc_set_freq(LEDC_LOW_SPEED_MODE, config_.timer, frequencyHz) != ESP_OK) {
    return false;
  }

  if (ledc_set_duty(LEDC_LOW_SPEED_MODE, config_.channel, halfDuty()) != ESP_OK ||
      ledc_update_duty(LEDC_LOW_SPEED_MODE, config_.channel) != ESP_OK) {
    return false;
  }

  const uint32_t configuredHz = ledc_get_freq(LEDC_LOW_SPEED_MODE, config_.timer);
  if (configuredHz == 0) {
    stop();
    return false;
  }

  actualFrequencyHz_ = static_cast<float>(configuredHz);
  return true;
}

void Esp32LedcPwmOutput::stop() {
  if (!begun_) {
    actualFrequencyHz_ = 0.0f;
    return;
  }

  ledc_set_duty(LEDC_LOW_SPEED_MODE, config_.channel, 0);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, config_.channel);
  actualFrequencyHz_ = 0.0f;
}

float Esp32LedcPwmOutput::frequencyHz() const {
  return actualFrequencyHz_;
}

float Esp32LedcPwmOutput::periodUs() const {
  return actualFrequencyHz_ > 0.0f ? 1000000.0f / actualFrequencyHz_ : 0.0f;
}

uint32_t Esp32LedcPwmOutput::halfDuty() const {
  return 1UL << (static_cast<uint8_t>(config_.resolution) - 1);
}
