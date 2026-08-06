#pragma once

#include <LevelWindow.h>

// MAX9814 microphone glue: ADC setup and windowed sampling.
void micBegin();

// Busy-samples one window (SOUND_SAMPLE_WINDOW_MS); windows run back-to-back
// in the loop so short claps are never missed.
LevelWindow micSampleWindow();

// Most recent window, for observers outside the loop (e.g. the HTTP API).
const LevelWindow &micLastWindow();
