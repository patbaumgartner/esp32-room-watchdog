#include <unity.h>

#include "PcmEncoder.h"

void setUp() {}
void tearDown() {}

void test_first_sample_establishes_silence_level()
{
    PcmEncoder encoder;
    TEST_ASSERT_EQUAL(0, encoder.encode(2700));
}

void test_audio_is_centered_around_the_established_bias()
{
    PcmEncoder encoder;
    encoder.encode(2700);

    TEST_ASSERT_INT16_WITHIN(8, 800, encoder.encode(2800));
    TEST_ASSERT_INT16_WITHIN(8, -800, encoder.encode(2600));
}

void test_adc_extremes_fit_without_pcm_overflow()
{
    PcmEncoder encoder;
    encoder.encode(4095);

    const int16_t low = encoder.encode(0);
    TEST_ASSERT_TRUE(low < -32000);
    TEST_ASSERT_TRUE(low >= INT16_MIN);
}

void test_out_of_range_input_is_clamped()
{
    PcmEncoder encoder;
    encoder.encode(0);

    const int16_t high = encoder.encode(5000);
    TEST_ASSERT_TRUE(high > 32000);
    TEST_ASSERT_TRUE(high <= INT16_MAX);
}

void test_reset_learns_a_new_bias()
{
    PcmEncoder encoder;
    encoder.encode(2700);
    encoder.encode(2800);
    encoder.reset();

    TEST_ASSERT_EQUAL(0, encoder.encode(1800));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_first_sample_establishes_silence_level);
    RUN_TEST(test_audio_is_centered_around_the_established_bias);
    RUN_TEST(test_adc_extremes_fit_without_pcm_overflow);
    RUN_TEST(test_out_of_range_input_is_clamped);
    RUN_TEST(test_reset_learns_a_new_bias);
    return UNITY_END();
}