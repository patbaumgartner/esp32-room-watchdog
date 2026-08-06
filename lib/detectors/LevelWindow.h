#pragma once

// Accumulates min/max over one window of 12-bit ADC readings; peakToPeak()
// is the loudness measure fed to SoundDetector. Pure logic, no hardware deps.
class LevelWindow
{
public:
    void add(int level)
    {
        if (level < min_)
        {
            min_ = level;
        }
        if (level > max_)
        {
            max_ = level;
        }
    }

    void reset()
    {
        min_ = INITIAL_MIN;
        max_ = INITIAL_MAX;
    }

    bool isEmpty() const { return max_ < min_; }
    int minLevel() const { return min_; }
    int maxLevel() const { return max_; }
    int peakToPeak() const { return isEmpty() ? 0 : max_ - min_; }

private:
    static constexpr int INITIAL_MIN = 4095;
    static constexpr int INITIAL_MAX = 0;

    int min_ = INITIAL_MIN;
    int max_ = INITIAL_MAX;
};
