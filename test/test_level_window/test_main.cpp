#include <unity.h>

#include "LevelWindow.h"

void setUp() {}
void tearDown() {}

void test_empty_window_has_zero_peak_to_peak()
{
    LevelWindow w;
    TEST_ASSERT_TRUE(w.isEmpty());
    TEST_ASSERT_EQUAL(0, w.peakToPeak());
}

void test_single_sample_gives_zero_swing()
{
    LevelWindow w;
    w.add(2600);
    TEST_ASSERT_FALSE(w.isEmpty());
    TEST_ASSERT_EQUAL(2600, w.minLevel());
    TEST_ASSERT_EQUAL(2600, w.maxLevel());
    TEST_ASSERT_EQUAL(0, w.peakToPeak());
}

void test_tracks_min_max_across_samples()
{
    LevelWindow w;
    w.add(2600);
    w.add(1500);
    w.add(3100);
    w.add(2700);
    TEST_ASSERT_EQUAL(1500, w.minLevel());
    TEST_ASSERT_EQUAL(3100, w.maxLevel());
    TEST_ASSERT_EQUAL(1600, w.peakToPeak());
}

void test_handles_adc_extremes()
{
    LevelWindow w;
    w.add(0);
    w.add(4095);
    TEST_ASSERT_EQUAL(4095, w.peakToPeak());
}

void test_reset_clears_window()
{
    LevelWindow w;
    w.add(0);
    w.add(4095);
    w.reset();
    TEST_ASSERT_TRUE(w.isEmpty());
    TEST_ASSERT_EQUAL(0, w.peakToPeak());
    w.add(2000);
    w.add(2500);
    TEST_ASSERT_EQUAL(500, w.peakToPeak());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_empty_window_has_zero_peak_to_peak);
    RUN_TEST(test_single_sample_gives_zero_swing);
    RUN_TEST(test_tracks_min_max_across_samples);
    RUN_TEST(test_handles_adc_extremes);
    RUN_TEST(test_reset_clears_window);
    return UNITY_END();
}
