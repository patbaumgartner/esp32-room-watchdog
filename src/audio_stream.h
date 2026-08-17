#pragma once

#include <ESPAsyncWebServer.h>

// Registers GET /audio.pcm on the API server, behind the same bearer-token
// middleware as everything else.
//
// The stream is an HTTP chunked response whose filler runs on the AsyncTCP
// task, so it never waits: when the ring buffer is momentarily empty it asks
// to be called again instead of blocking. The TCP ack clock keeps it moving —
// the filler only reports "empty" after it has just handed over data, so a
// segment is always in flight whose ack drives the next call, by which time
// the ADC has produced more samples.
void audioStreamAttach(AsyncWebServer &server, AsyncMiddleware &authentication);
