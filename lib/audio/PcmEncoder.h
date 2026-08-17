#pragma once

#include <stdint.h>

// Removes the microphone's DC bias and maps the complete 12-bit ADC range
// into signed 16-bit PCM without introducing clipping.
class PcmEncoder
{
public:
    int16_t encode(uint16_t sample)
    {
        if (sample > ADC_MAX)
        {
            sample = ADC_MAX;
        }

        const int32_t sampleQ8 = static_cast<int32_t>(sample) << FRACTION_BITS;
        if (!initialized_)
        {
            dcQ8_ = sampleQ8;
            initialized_ = true;
        }
        else
        {
            dcQ8_ += (sampleQ8 - dcQ8_) >> DC_FILTER_SHIFT;
        }

        const int32_t centered = (sampleQ8 - dcQ8_) >> FRACTION_BITS;
        return static_cast<int16_t>(centered << PCM_SCALE_SHIFT);
    }

    void reset()
    {
        initialized_ = false;
        dcQ8_ = 0;
    }

private:
    static constexpr uint16_t ADC_MAX = 4095;
    static constexpr int FRACTION_BITS = 8;
    static constexpr int DC_FILTER_SHIFT = 12;
    static constexpr int PCM_SCALE_SHIFT = 3;

    bool initialized_ = false;
    int32_t dcQ8_ = 0;
};