#pragma once

// The synchronous HTTP server that serves GET /audio.pcm on AUDIO_PORT.
//
// It lives in its own translation unit on purpose: WebServer.h and
// ESPAsyncWebServer.h both define HTTP_GET, so the two cannot meet in one
// file. The split is also honest about the design — a recording holds its
// socket for minutes, which is exactly what an async handler must never do.
void audioApiBegin();
