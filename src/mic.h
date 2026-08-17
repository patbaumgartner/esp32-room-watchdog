#pragma once

#include <stddef.h>
#include <stdint.h>

#include <LevelWindow.h>

// Starts hardware-timed ADC/DMA sampling of the MAX9814.
bool micBegin();

// Waits for the next completed SOUND_SAMPLE_WINDOW_MS level window.
LevelWindow micSampleWindow();

// Most recent window, for observers outside the loop (e.g. the HTTP API).
LevelWindow micLastWindow();

// Controls the single-consumer PCM ring buffer used by the network streamer.
bool micStartPcmStream();
void micStopPcmStream();
bool micPcmStreaming();
uint32_t micDroppedSamples();

// Copies whatever is buffered and returns immediately, so the caller can be an
// async callback that must never wait. Returns bytes written, 0 when empty.
// Byte-oriented because the destination is a network buffer at an arbitrary
// offset, which int16_t may not be aligned to.
size_t micTryReadPcm(uint8_t *destination, size_t maxBytes);
