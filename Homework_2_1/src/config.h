#pragma once
#include <Arduino.h>
namespace config{
    constexpr uint32_t BAUDRATE = 115200;

    constexpr uint8_t LED_PIN = 4;
    constexpr uint8_t BUTTON_PIN = 5;
    constexpr uint32_t BLINKING_TIME = 200;
}
