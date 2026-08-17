#pragma once

#include <stddef.h>
#include <stdint.h>

// Builds LD2412 configuration command frames (protocol PDF §2.1-2.2):
//   FD FC FB FA | len(2, LE) | cmdWord(2, LE) | value[N] | 04 03 02 01
//
// Commands must be wrapped in enableConfig() ... endConfig().
// Pure logic: no hardware deps, unit-tested against the PDF's example frames.
class Ld2412Commands
{
public:
    static constexpr size_t MAX_FRAME = 32;
    static constexpr size_t GATE_COUNT = 14;

    static size_t enableConfig(uint8_t *out)
    {
        const uint8_t value[] = {0x01, 0x00};
        return frame(out, 0x00FF, value, sizeof(value));
    }

    static size_t endConfig(uint8_t *out)
    {
        return frame(out, 0x00FE, nullptr, 0);
    }

    // One sensitivity per gate (0-100); a target needs energy above it, so
    // higher values make the radar less sensitive.
    static size_t motionSensitivity(uint8_t *out, const uint8_t gates[GATE_COUNT])
    {
        return frame(out, 0x0003, gates, GATE_COUNT);
    }

    static size_t staticSensitivity(uint8_t *out, const uint8_t gates[GATE_COUNT])
    {
        return frame(out, 0x0004, gates, GATE_COUNT);
    }

    // polarity: 0 = OUT high when occupied (default), 1 = inverted.
    static size_t basicParams(uint8_t *out, uint8_t minGate, uint8_t maxGate,
                              uint16_t unmannedSeconds, uint8_t polarity)
    {
        const uint8_t value[] = {minGate, maxGate,
                                 static_cast<uint8_t>(unmannedSeconds & 0xFF),
                                 static_cast<uint8_t>(unmannedSeconds >> 8),
                                 polarity};
        return frame(out, 0x0002, value, sizeof(value));
    }

    // Starts 10s after the command; calibrates against the (empty) room and
    // stores the result in the module's flash.
    static size_t backgroundCorrection(uint8_t *out)
    {
        return frame(out, 0x000B, nullptr, 0);
    }

private:
    // Callers pass a uint8_t[MAX_FRAME]. A frame is 12 bytes of envelope
    // (header, length, command word, footer) plus the value, and the gate
    // arrays are the longest value we send.
    static_assert(12 + GATE_COUNT <= MAX_FRAME, "MAX_FRAME is too small for a gate command");

    static size_t frame(uint8_t *out, uint16_t cmdWord, const uint8_t *value, size_t valueLen)
    {
        size_t i = 0;
        out[i++] = 0xFD;
        out[i++] = 0xFC;
        out[i++] = 0xFB;
        out[i++] = 0xFA;
        const uint16_t dataLen = static_cast<uint16_t>(2 + valueLen);
        out[i++] = static_cast<uint8_t>(dataLen & 0xFF);
        out[i++] = static_cast<uint8_t>(dataLen >> 8);
        out[i++] = static_cast<uint8_t>(cmdWord & 0xFF);
        out[i++] = static_cast<uint8_t>(cmdWord >> 8);
        for (size_t v = 0; v < valueLen; ++v)
        {
            out[i++] = value[v];
        }
        out[i++] = 0x04;
        out[i++] = 0x03;
        out[i++] = 0x02;
        out[i++] = 0x01;
        return i;
    }
};
