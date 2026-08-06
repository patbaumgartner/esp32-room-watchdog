#include "calibration_button.h"

#include <Arduino.h>

#include "config.h"
#include "notifications.h"

void calibrationButtonBegin()
{
    pinMode(PIN_BOOT_BUTTON, INPUT_PULLUP);
}

void calibrationButtonPoll()
{
    static uint32_t pressedSinceMs = 0;
    if (digitalRead(PIN_BOOT_BUTTON) == HIGH)
    {
        pressedSinceMs = 0;
        return;
    }
    if (pressedSinceMs == 0)
    {
        pressedSinceMs = millis();
        return;
    }
    if (millis() - pressedSinceMs >= 1000)
    {
        pressedSinceMs = 0;
        startCalibration();
    }
}
