#pragma once

#include <stdint.h>

// Parses the LD2412's periodic UART data frames into target reports.
//
// Frame layout (protocol PDF in docs/datasheets/HLK-2412):
//   F4 F3 F2 F1 | len(2, LE) | data[len] | F8 F7 F6 F5
//   data: mode(1) AA(1) state(1) movDist(2, LE, cm) movEnergy(1)
//         statDist(2, LE, cm) statEnergy(1) ...trailer
//
// Feed bytes one at a time; feed() returns true when a complete, valid
// report was decoded (frames may span multiple UART reads). Garbage and
// truncated frames resynchronize on the next header.
//
// Pure logic: no hardware deps, unit-tested natively.
class Ld2412Parser
{
public:
    struct Report
    {
        uint8_t targetState = 0; // bit0 = moving target, bit1 = stationary target
        uint16_t movingDistanceCm = 0;
        uint8_t movingEnergy = 0;
        uint16_t stationaryDistanceCm = 0;
        uint8_t stationaryEnergy = 0;

        bool hasTarget() const { return targetState != 0; }
        bool isMoving() const { return (targetState & 0x01) != 0; }
        bool isStationary() const { return (targetState & 0x02) != 0; }

        // Moving target wins: it is the fresher measurement of a person.
        uint16_t primaryDistanceCm() const
        {
            if (isMoving())
            {
                return movingDistanceCm;
            }
            if (isStationary())
            {
                return stationaryDistanceCm;
            }
            return 0;
        }
    };

    // Consumes one byte; true when a complete report is available in report().
    bool feed(uint8_t byte)
    {
        switch (state_)
        {
        case State::Header:
            matchHeader(byte);
            return false;

        case State::Length:
            if (lengthPos_ == 0)
            {
                length_ = byte;
                lengthPos_ = 1;
            }
            else
            {
                length_ |= static_cast<uint16_t>(byte) << 8;
                if (length_ < MIN_DATA_LEN || length_ > MAX_DATA_LEN)
                {
                    reset();
                }
                else
                {
                    state_ = State::Data;
                    dataPos_ = 0;
                }
            }
            return false;

        case State::Data:
            data_[dataPos_++] = byte;
            if (dataPos_ == length_)
            {
                state_ = State::Footer;
                footerPos_ = 0;
            }
            return false;

        case State::Footer:
            if (byte != footerByte(footerPos_))
            {
                reset();
                matchHeader(byte); // byte may already start the next header
                return false;
            }
            if (++footerPos_ == 4)
            {
                const bool valid = decodeData();
                reset();
                return valid;
            }
            return false;
        }
        return false;
    }

    const Report &report() const { return report_; }

private:
    enum class State
    {
        Header,
        Length,
        Data,
        Footer
    };

    static constexpr uint16_t MIN_DATA_LEN = 9;  // through statEnergy
    static constexpr uint16_t MAX_DATA_LEN = 64; // resync guard against garbage

    static constexpr uint8_t MODE_ENGINEERING = 0x01;
    static constexpr uint8_t MODE_BASIC = 0x02;
    static constexpr uint8_t DATA_HEAD_MARKER = 0xAA;

    // Function-local arrays keep the class header-only under C++11.
    static uint8_t headerByte(uint8_t i)
    {
        static const uint8_t bytes[4] = {0xF4, 0xF3, 0xF2, 0xF1};
        return bytes[i];
    }

    static uint8_t footerByte(uint8_t i)
    {
        static const uint8_t bytes[4] = {0xF8, 0xF7, 0xF6, 0xF5};
        return bytes[i];
    }

    void matchHeader(uint8_t byte)
    {
        if (byte == headerByte(headerPos_))
        {
            if (++headerPos_ == 4)
            {
                state_ = State::Length;
                lengthPos_ = 0;
            }
        }
        else
        {
            headerPos_ = byte == headerByte(0) ? 1 : 0;
        }
    }

    bool decodeData()
    {
        const bool knownMode = data_[0] == MODE_BASIC || data_[0] == MODE_ENGINEERING;
        if (!knownMode || data_[1] != DATA_HEAD_MARKER)
        {
            return false;
        }

        report_.targetState = data_[2];
        report_.movingDistanceCm = data_[3] | static_cast<uint16_t>(data_[4]) << 8;
        report_.movingEnergy = data_[5];
        report_.stationaryDistanceCm = data_[6] | static_cast<uint16_t>(data_[7]) << 8;
        report_.stationaryEnergy = data_[8];
        return true;
    }

    void reset()
    {
        state_ = State::Header;
        headerPos_ = 0;
        lengthPos_ = 0;
        dataPos_ = 0;
        footerPos_ = 0;
        length_ = 0;
    }

    State state_ = State::Header;
    uint8_t headerPos_ = 0;
    uint8_t lengthPos_ = 0;
    uint16_t length_ = 0;
    uint16_t dataPos_ = 0;
    uint8_t footerPos_ = 0;
    uint8_t data_[MAX_DATA_LEN] = {};
    Report report_;
};
