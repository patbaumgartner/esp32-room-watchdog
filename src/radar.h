#pragma once

#include <Arduino.h>
#include <Ld2412Parser.h>

// HLK-LD2412 radar glue: presence pin + UART frame decoding.
void radarBegin();

// Drains pending UART bytes into the frame parser; call every loop pass.
void radarPoll();

// Writes the sensitivity/range tuning from config.h to the module.
void radarApplyTuning();

// Starts the module's dynamic background correction: it calibrates against
// the room 10s after the call (leave the room!) and stores the result.
void radarCalibrateBackground();

// Raw debounce-free state of the radar's OUT pin.
bool radarPresenceDetected();

// Latest decoded target report (distances in cm, energies 0-100).
const Ld2412Parser::Report &radarReport();

// Human-readable target position, e.g. "1.5m (moving)"; "" when no target.
String radarDescribeTarget();
