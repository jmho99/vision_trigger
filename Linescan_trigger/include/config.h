#pragma once

#include <Arduino.h>

// XIAO ESP32-C3 pin names and corresponding GPIO numbers:
// D2 = GPIO4, D3 = GPIO5, D4 = GPIO6, D5 = GPIO7
constexpr gpio_num_t LINE_SCAN_TRIGGER_GPIO = GPIO_NUM_4;
constexpr gpio_num_t RGB_CAMERA_TRIGGER_GPIO = GPIO_NUM_5;
constexpr gpio_num_t CAN_TX_GPIO = GPIO_NUM_6;
constexpr gpio_num_t CAN_RX_GPIO = GPIO_NUM_7;

constexpr float DISTANCE_PER_TRIGGER_MM = 1.0f;
constexpr float MAX_CAMERA_LINE_RATE_HZ = 60000.0f;
constexpr float RGB_CAMERA_TRIGGER_HZ = 20.0f;

constexpr uint32_t SERIAL_BAUD_RATE = 115200;
constexpr uint8_t SERIAL_PACKET_HEADER = 0xAA;
constexpr uint8_t SERIAL_FLAG_TRIG_ON_STAT_OFF = 1;
constexpr uint8_t SERIAL_FLAG_TRIG_OFF_STAT_OFF = 2;
constexpr uint8_t SERIAL_FLAG_TRIG_ON_STAT_ON = 3;
constexpr uint8_t SERIAL_FLAG_TRIG_OFF_STAT_ON = 4;

// 0: use USB Serial binary packets (current test mode)
// 1: periodically request OBD-II PID 0x0D through TWAI/CAN
constexpr bool ENABLE_CAN_SPEED_INPUT = false;

constexpr uint32_t CAN_SPEED_REQUEST_INTERVAL_MS = 100;
constexpr uint32_t CAN_SPEED_TIMEOUT_MS = 1000;
