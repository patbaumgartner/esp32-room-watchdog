#pragma once

#include <LevelWindow.h>

// Event detection and delivery routing. Owns the detectors (lib/detectors)
// and decides which sink each event belongs to: alerts reach the phone via
// the queued Gotify push, continuous updates only reach the live socket.
void notifyBootOnline();
void notifyPresenceChanges(bool presentNow);
void notifyMovement();
void notifyLoudSounds(const LevelWindow &mic);

// Asks for radar background calibration. Safe from any task: the request is
// only a flag, because the command itself blocks on UART acknowledgements for
// ~300ms and must not run inside a network callback.
void requestCalibration();

// Runs a pending calibration request; call once per loop pass.
void pollCalibrationRequest();
