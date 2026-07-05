#include <Arduino.h>
#include "config.h"
#include "Led.hpp"
#include "LedState.hpp"
#include "LedMode.hpp"

LedState state = LedState::OFF;
Led led(config::LED_PIN);
LedMode ledMode = LedMode::Blinking;

volatile bool buttonPressed = false;

void IRAM_ATTR buttonISR()
{
  buttonPressed = true;
}

void setup()
{
  Serial.begin(config::BAUDRATE);
  Serial.println("_________START_______________");
  led.init();
  pinMode(config::BUTTON_PIN, INPUT_PULLDOWN);
  attachInterrupt(
      digitalPinToInterrupt(config::BUTTON_PIN),
      buttonISR,
      RISING);
}

void loop()
{
  if(buttonPressed){
    switch (ledMode)
    {
      case LedMode::Blinking:
        ledMode = LedMode::AlwaysOn;
        break;
      case LedMode::AlwaysOn:
        ledMode = LedMode::AlwaysOff;
        break;
      case LedMode::AlwaysOff:
        ledMode = LedMode::Blinking;
        break;
    }

    buttonPressed = false;
  }

  switch (ledMode)
    {
      case LedMode::Blinking:
       led.blink(config::BLINKING_TIME);
       break;
      case LedMode::AlwaysOn:
        led.turnOn();
        break;
      case LedMode::AlwaysOff:
        led.turnOff();
        break;
    }
}
