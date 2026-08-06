// Firmware entry point — pure composition. Sensor glue lives in mic/radar,
// event decisions in lib/, delivery in notifications. See docs/architecture.md
#include <Arduino.h>

#include <LevelWindow.h>

#include "api.h"
#include "calibration_button.h"
#include "mic.h"
#include "net.h"
#include "notifications.h"
#include "radar.h"
#include "status_log.h"

void setup()
{
  Serial.begin(115200);
  micBegin();
  radarBegin();
  calibrationButtonBegin();

  Serial.println("Sensor node booting...");
  radarApplyTuning();
  connectWifi();
  apiBegin();
  notifyBootOnline();
}

void loop()
{
  const LevelWindow mic = micSampleWindow();
  const bool presentNow = radarPresenceDetected();
  radarPoll();

  calibrationButtonPoll();
  apiPoll();
  notifyPresenceChanges(presentNow);
  notifyMovement();
  notifyLoudSounds(mic);
  logStatusEverySecond(presentNow, mic);
}
