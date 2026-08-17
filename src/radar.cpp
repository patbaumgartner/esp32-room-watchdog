#include "radar.h"

#include <Ld2412Commands.h>

#include "config.h"

namespace
{
    Ld2412Parser parser;

    // The parser fills its report field by field, so observers on the API and
    // WebSocket tasks must not read it directly. radarPoll() copies a completed
    // frame here in one go instead.
    Ld2412Parser::Report published;
    portMUX_TYPE reportMux = portMUX_INITIALIZER_UNLOCKED;

    void sendFrame(const uint8_t *frame, size_t length)
    {
        Serial1.write(frame, length);
        Serial1.flush();
        delay(100); // module needs a moment to ACK before the next command
    }

    void sendCommand(size_t (*build)(uint8_t *))
    {
        uint8_t frame[Ld2412Commands::MAX_FRAME];
        sendFrame(frame, build(frame));
    }
}

void radarBegin()
{
    Serial1.begin(LD2412_BAUD, SERIAL_8N1, PIN_LD2412_RX, PIN_LD2412_TX);
    pinMode(PIN_LD2412_OUT, INPUT);
}

void radarApplyTuning()
{
    uint8_t frame[Ld2412Commands::MAX_FRAME];
    sendCommand(Ld2412Commands::enableConfig);
    sendFrame(frame, Ld2412Commands::basicParams(frame, RADAR_MIN_GATE, RADAR_MAX_GATE,
                                                 RADAR_UNMANNED_SECONDS, 0));
    sendFrame(frame, Ld2412Commands::motionSensitivity(frame, RADAR_MOTION_SENSITIVITY));
    sendFrame(frame, Ld2412Commands::staticSensitivity(frame, RADAR_STATIC_SENSITIVITY));
    sendCommand(Ld2412Commands::endConfig);
    Serial.println("radar: tuning applied");
}

void radarCalibrateBackground()
{
    sendCommand(Ld2412Commands::enableConfig);
    sendCommand(Ld2412Commands::backgroundCorrection);
    sendCommand(Ld2412Commands::endConfig);
    Serial.println("radar: background correction starts in 10s");
}

void radarPoll()
{
    while (Serial1.available())
    {
        if (parser.feed(Serial1.read()))
        {
            portENTER_CRITICAL(&reportMux);
            published = parser.report();
            portEXIT_CRITICAL(&reportMux);
        }
    }
}

bool radarPresenceDetected()
{
    return digitalRead(PIN_LD2412_OUT) == HIGH;
}

Ld2412Parser::Report radarReport()
{
    portENTER_CRITICAL(&reportMux);
    const Ld2412Parser::Report report = published;
    portEXIT_CRITICAL(&reportMux);
    return report;
}

String radarDescribeTarget()
{
    const Ld2412Parser::Report report = radarReport();
    if (!report.hasTarget())
    {
        return "";
    }
    const float meters = report.primaryDistanceCm() / 100.0f;
    return String(meters, 1) + "m" + (report.isMoving() ? " (moving)" : " (still)");
}
