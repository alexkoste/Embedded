#pragma once
#include <Arduino.h>
#include "LedState.hpp"

class Led
{
private:
    uint8_t pin_;
    LedState state_;
    unsigned long previousTime_;

public:
    Led(uint8_t pin) : pin_(pin), state_(LedState::OFF) { previousTime_ = 0; };
    void init()
    {
        pinMode(pin_, OUTPUT);
        setState(state_);
    };

    void setState(LedState state)
    {
        digitalWrite(pin_, (state == LedState::ON) ? HIGH : LOW);
    };

    void toggleState()
    {
        state_ = (state_ == LedState::ON) ? LedState::OFF : LedState::ON;
        setState(state_);
    };

    void blink(uint32_t duration)
    {

        unsigned long currentTime = millis();

        if (currentTime - previousTime_ >= duration)
        {
            previousTime_ = currentTime;

            state_ = (state_ == LedState::ON)
                         ? LedState::OFF
                         : LedState::ON;
            setState(state_);
        }
    }

    void turnOn()
    {
        setState(LedState::ON);
    };

    void turnOff()
    {

        setState(LedState::OFF);
    };
};