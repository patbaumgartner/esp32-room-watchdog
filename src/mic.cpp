#include "mic.h"

#include <Arduino.h>

#include "config.h"

namespace
{
    LevelWindow lastWindow;
}

void micBegin()
{
    analogReadResolution(12); // 0-4095
}

LevelWindow micSampleWindow()
{
    LevelWindow window;
    const uint32_t start = millis();
    while (millis() - start < SOUND_SAMPLE_WINDOW_MS)
    {
        window.add(analogRead(PIN_MIC_OUT));
    }
    lastWindow = window;
    return window;
}

const LevelWindow &micLastWindow()
{
    return lastWindow;
}
