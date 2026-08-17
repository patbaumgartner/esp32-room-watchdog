#include <unity.h>

#include "TelemetryGate.h"

void setUp() {}
void tearDown() {}

void test_first_frame_is_always_sent()
{
    const TelemetryGate gate(100, 2000);
    TEST_ASSERT_TRUE(gate.shouldSend(0xAAAA, 0));
}

void test_unchanged_payload_stays_quiet()
{
    TelemetryGate gate(100, 2000);
    gate.sent(0xAAAA, 1000);
    TEST_ASSERT_FALSE(gate.shouldSend(0xAAAA, 1500));
}

void test_change_is_sent_once_the_min_interval_passed()
{
    TelemetryGate gate(100, 2000);
    gate.sent(0xAAAA, 1000);
    TEST_ASSERT_FALSE(gate.shouldSend(0xBBBB, 1099));
    TEST_ASSERT_TRUE(gate.shouldSend(0xBBBB, 1100));
}

void test_heartbeat_fires_without_any_change()
{
    TelemetryGate gate(100, 2000);
    gate.sent(0xAAAA, 1000);
    TEST_ASSERT_FALSE(gate.shouldSend(0xAAAA, 2999));
    TEST_ASSERT_TRUE(gate.shouldSend(0xAAAA, 3000));
}

void test_dropped_frame_retries()
{
    TelemetryGate gate(100, 2000);
    gate.sent(0xAAAA, 1000);
    TEST_ASSERT_TRUE(gate.shouldSend(0xBBBB, 1200));
    // No sent() -> the socket refused the frame, so it stays pending.
    TEST_ASSERT_TRUE(gate.shouldSend(0xBBBB, 1201));
}

void test_reset_serves_a_reconnecting_client_immediately()
{
    TelemetryGate gate(100, 2000);
    gate.sent(0xAAAA, 1000);
    TEST_ASSERT_FALSE(gate.shouldSend(0xAAAA, 1050));
    gate.reset();
    TEST_ASSERT_TRUE(gate.shouldSend(0xAAAA, 1050));
}

void test_millis_rollover_does_not_stall_the_stream()
{
    TelemetryGate gate(100, 2000);
    const uint32_t beforeRollover = 0xFFFFFF00u;
    gate.sent(0xAAAA, beforeRollover);
    TEST_ASSERT_TRUE(gate.shouldSend(0xBBBB, beforeRollover + 200)); // wraps past zero
}

void test_fingerprint_distinguishes_payloads()
{
    const uint8_t a[] = {1, 2, 3};
    const uint8_t b[] = {1, 2, 4};
    TEST_ASSERT_EQUAL_UINT32(telemetryFingerprint(a, sizeof(a)),
                             telemetryFingerprint(a, sizeof(a)));
    TEST_ASSERT_NOT_EQUAL(telemetryFingerprint(a, sizeof(a)),
                          telemetryFingerprint(b, sizeof(b)));
}

void test_fingerprint_is_order_sensitive()
{
    const uint8_t forward[] = {1, 2};
    const uint8_t reversed[] = {2, 1};
    TEST_ASSERT_NOT_EQUAL(telemetryFingerprint(forward, sizeof(forward)),
                          telemetryFingerprint(reversed, sizeof(reversed)));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_first_frame_is_always_sent);
    RUN_TEST(test_unchanged_payload_stays_quiet);
    RUN_TEST(test_change_is_sent_once_the_min_interval_passed);
    RUN_TEST(test_heartbeat_fires_without_any_change);
    RUN_TEST(test_dropped_frame_retries);
    RUN_TEST(test_reset_serves_a_reconnecting_client_immediately);
    RUN_TEST(test_millis_rollover_does_not_stall_the_stream);
    RUN_TEST(test_fingerprint_distinguishes_payloads);
    RUN_TEST(test_fingerprint_is_order_sensitive);
    return UNITY_END();
}
