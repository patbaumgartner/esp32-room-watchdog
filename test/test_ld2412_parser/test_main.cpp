#include <unity.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "Ld2412Parser.h"

void setUp() {}
void tearDown() {}

// Basic-mode frame: moving+stationary target, moving 81cm/60%, stationary 100cm/40%.
static const uint8_t VALID_FRAME[] = {
    0xF4, 0xF3, 0xF2, 0xF1,  // header
    0x0D, 0x00,              // data length = 13
    0x02, 0xAA,              // basic mode, head marker
    0x03,                    // state: moving + stationary
    0x51, 0x00, 0x3C,        // moving: 81cm, energy 60
    0x64, 0x00, 0x28,        // stationary: 100cm, energy 40
    0x60, 0x00,              // detection distance (ignored)
    0x55, 0x00,              // trailer
    0xF8, 0xF7, 0xF6, 0xF5}; // footer

static bool feedAll(Ld2412Parser &parser, const uint8_t *bytes, size_t count)
{
    bool gotReport = false;
    for (size_t i = 0; i < count; ++i)
    {
        gotReport = parser.feed(bytes[i]) || gotReport;
    }
    return gotReport;
}

void test_parses_valid_basic_frame()
{
    Ld2412Parser parser;
    TEST_ASSERT_TRUE(feedAll(parser, VALID_FRAME, sizeof(VALID_FRAME)));

    const Ld2412Parser::Report &r = parser.report();
    TEST_ASSERT_TRUE(r.hasTarget());
    TEST_ASSERT_TRUE(r.isMoving());
    TEST_ASSERT_TRUE(r.isStationary());
    TEST_ASSERT_EQUAL_UINT16(81, r.movingDistanceCm);
    TEST_ASSERT_EQUAL_UINT8(60, r.movingEnergy);
    TEST_ASSERT_EQUAL_UINT16(100, r.stationaryDistanceCm);
    TEST_ASSERT_EQUAL_UINT8(40, r.stationaryEnergy);
    TEST_ASSERT_EQUAL_UINT16(81, r.primaryDistanceCm()); // moving wins
}

void test_stationary_only_uses_stationary_distance()
{
    uint8_t frame[sizeof(VALID_FRAME)];
    memcpy(frame, VALID_FRAME, sizeof(VALID_FRAME));
    frame[8] = 0x02; // state: stationary only

    Ld2412Parser parser;
    TEST_ASSERT_TRUE(feedAll(parser, frame, sizeof(frame)));
    TEST_ASSERT_FALSE(parser.report().isMoving());
    TEST_ASSERT_EQUAL_UINT16(100, parser.report().primaryDistanceCm());
}

void test_no_target_reports_zero_distance()
{
    uint8_t frame[sizeof(VALID_FRAME)];
    memcpy(frame, VALID_FRAME, sizeof(VALID_FRAME));
    frame[8] = 0x00; // state: no target

    Ld2412Parser parser;
    TEST_ASSERT_TRUE(feedAll(parser, frame, sizeof(frame)));
    TEST_ASSERT_FALSE(parser.report().hasTarget());
    TEST_ASSERT_EQUAL_UINT16(0, parser.report().primaryDistanceCm());
}

void test_resyncs_after_garbage_prefix()
{
    const uint8_t garbage[] = {0x00, 0xFF, 0xF4, 0x13, 0xF4, 0xF3, 0x99};

    Ld2412Parser parser;
    TEST_ASSERT_FALSE(feedAll(parser, garbage, sizeof(garbage)));
    TEST_ASSERT_TRUE(feedAll(parser, VALID_FRAME, sizeof(VALID_FRAME)));
    TEST_ASSERT_EQUAL_UINT16(81, parser.report().movingDistanceCm);
}

void test_rejects_frame_with_bad_footer()
{
    uint8_t frame[sizeof(VALID_FRAME)];
    memcpy(frame, VALID_FRAME, sizeof(VALID_FRAME));
    frame[sizeof(VALID_FRAME) - 1] = 0x00; // corrupt last footer byte

    Ld2412Parser parser;
    TEST_ASSERT_FALSE(feedAll(parser, frame, sizeof(frame)));
    // Parser must recover and accept the next clean frame.
    TEST_ASSERT_TRUE(feedAll(parser, VALID_FRAME, sizeof(VALID_FRAME)));
}

void test_rejects_command_ack_frames()
{
    // Command ACKs use the same envelope but no 0xAA data head marker.
    uint8_t frame[sizeof(VALID_FRAME)];
    memcpy(frame, VALID_FRAME, sizeof(VALID_FRAME));
    frame[7] = 0x00; // not DATA_HEAD_MARKER

    Ld2412Parser parser;
    TEST_ASSERT_FALSE(feedAll(parser, frame, sizeof(frame)));
}

void test_resyncs_on_absurd_length()
{
    const uint8_t bogus[] = {0xF4, 0xF3, 0xF2, 0xF1, 0xFF, 0xFF};

    Ld2412Parser parser;
    TEST_ASSERT_FALSE(feedAll(parser, bogus, sizeof(bogus)));
    TEST_ASSERT_TRUE(feedAll(parser, VALID_FRAME, sizeof(VALID_FRAME)));
}

void test_back_to_back_frames_both_parse()
{
    Ld2412Parser parser;
    TEST_ASSERT_TRUE(feedAll(parser, VALID_FRAME, sizeof(VALID_FRAME)));

    uint8_t second[sizeof(VALID_FRAME)];
    memcpy(second, VALID_FRAME, sizeof(VALID_FRAME));
    second[9] = 0xC8; // moving distance -> 200cm
    TEST_ASSERT_TRUE(feedAll(parser, second, sizeof(second)));
    TEST_ASSERT_EQUAL_UINT16(200, parser.report().movingDistanceCm);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_parses_valid_basic_frame);
    RUN_TEST(test_stationary_only_uses_stationary_distance);
    RUN_TEST(test_no_target_reports_zero_distance);
    RUN_TEST(test_resyncs_after_garbage_prefix);
    RUN_TEST(test_rejects_frame_with_bad_footer);
    RUN_TEST(test_rejects_command_ack_frames);
    RUN_TEST(test_resyncs_on_absurd_length);
    RUN_TEST(test_back_to_back_frames_both_parse);
    return UNITY_END();
}
