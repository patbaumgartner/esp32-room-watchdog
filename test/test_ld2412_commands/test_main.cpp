#include <unity.h>

#include <string.h>

#include "Ld2412Commands.h"

void setUp() {}
void tearDown() {}

// Expected frames come verbatim from the protocol PDF's "Send data" examples.

static void assertFrame(const uint8_t *expected, size_t expectedLen,
                        const uint8_t *actual, size_t actualLen)
{
    TEST_ASSERT_EQUAL_size_t(expectedLen, actualLen);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, actual, expectedLen);
}

void test_enable_config_matches_datasheet_example()
{
    const uint8_t expected[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x04, 0x00,
                                0xFF, 0x00, 0x01, 0x00,
                                0x04, 0x03, 0x02, 0x01};
    uint8_t out[Ld2412Commands::MAX_FRAME];
    assertFrame(expected, sizeof(expected), out, Ld2412Commands::enableConfig(out));
}

void test_end_config_matches_datasheet_example()
{
    const uint8_t expected[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x02, 0x00,
                                0xFE, 0x00,
                                0x04, 0x03, 0x02, 0x01};
    uint8_t out[Ld2412Commands::MAX_FRAME];
    assertFrame(expected, sizeof(expected), out, Ld2412Commands::endConfig(out));
}

void test_basic_params_matches_datasheet_example()
{
    // minGate=1, maxGate=12(0x0C), unmanned=5s, polarity=0
    const uint8_t expected[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x07, 0x00,
                                0x02, 0x00, 0x01, 0x0C, 0x05, 0x00, 0x00,
                                0x04, 0x03, 0x02, 0x01};
    uint8_t out[Ld2412Commands::MAX_FRAME];
    assertFrame(expected, sizeof(expected), out,
                Ld2412Commands::basicParams(out, 1, 12, 5, 0));
}

void test_basic_params_encodes_unmanned_seconds_little_endian()
{
    // 300s = 0x012C: low byte first. Also checks polarity is passed through.
    const uint8_t expected[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x07, 0x00,
                                0x02, 0x00, 0x01, 0x08, 0x2C, 0x01, 0x01,
                                0x04, 0x03, 0x02, 0x01};
    uint8_t out[Ld2412Commands::MAX_FRAME];
    assertFrame(expected, sizeof(expected), out,
                Ld2412Commands::basicParams(out, 1, 8, 300, 1));
}

void test_motion_sensitivity_matches_datasheet_example()
{
    const uint8_t gates[14] = {0x00, 0x23, 0x23, 0x23, 0x19, 0x19, 0x19,
                               0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19};
    const uint8_t expected[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x10, 0x00,
                                0x03, 0x00,
                                0x00, 0x23, 0x23, 0x23, 0x19, 0x19, 0x19,
                                0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19,
                                0x04, 0x03, 0x02, 0x01};
    uint8_t out[Ld2412Commands::MAX_FRAME];
    assertFrame(expected, sizeof(expected), out,
                Ld2412Commands::motionSensitivity(out, gates));
}

void test_static_sensitivity_uses_command_word_0004()
{
    const uint8_t gates[14] = {0};
    uint8_t out[Ld2412Commands::MAX_FRAME];
    const size_t len = Ld2412Commands::staticSensitivity(out, gates);
    TEST_ASSERT_EQUAL_UINT8(0x04, out[6]); // command word LSB
    TEST_ASSERT_EQUAL_UINT8(0x00, out[7]);
    TEST_ASSERT_EQUAL_size_t(26, len);
}

void test_background_correction_matches_datasheet_example()
{
    const uint8_t expected[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x02, 0x00,
                                0x0B, 0x00,
                                0x04, 0x03, 0x02, 0x01};
    uint8_t out[Ld2412Commands::MAX_FRAME];
    assertFrame(expected, sizeof(expected), out, Ld2412Commands::backgroundCorrection(out));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_enable_config_matches_datasheet_example);
    RUN_TEST(test_end_config_matches_datasheet_example);
    RUN_TEST(test_basic_params_matches_datasheet_example);
    RUN_TEST(test_basic_params_encodes_unmanned_seconds_little_endian);
    RUN_TEST(test_motion_sensitivity_matches_datasheet_example);
    RUN_TEST(test_static_sensitivity_uses_command_word_0004);
    RUN_TEST(test_background_correction_matches_datasheet_example);
    return UNITY_END();
}
