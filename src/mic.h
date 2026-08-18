#pragma once

#include <stddef.h>
#include <stdint.h>

#include <LevelWindow.h>

// Starts hardware-timed ADC/DMA sampling of the MAX9814.
bool micBegin();

// Waits for the next completed SOUND_SAMPLE_WINDOW_MS level window. Returns an
// empty window if the ADC stops delivering, so the caller's loop keeps running.
LevelWindow micSampleWindow();

// Most recent window, for observers outside the loop (e.g. the HTTP API).
LevelWindow micLastWindow();

// Controls the single-consumer PCM ring buffer used by the network streamer.
bool micStartPcmStream();
void micStopPcmStream();
size_t micReadPcm(int16_t *samples, size_t maxSamples);
bool micPcmStreaming();
uint32_t micDroppedSamples();
