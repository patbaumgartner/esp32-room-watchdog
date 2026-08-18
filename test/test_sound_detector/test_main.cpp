#include <unity.h>

#include "SoundDetector.h"

void setUp() {}
void tearDown() {}

void test_below_threshold_never_notifies()
{
    SoundDetector d(800, 15000);
    TEST_ASSERT_FALSE(d.shouldNotify(0, 1000));
    TEST_ASSERT_FALSE(d.shouldNotify(799, 1000));
}

void test_at_threshold_notifies()
{
    SoundDetector d(800, 15000);
    TEST_ASSERT_TRUE(d.shouldNotify(800, 1000));
    TEST_ASSERT_TRUE(d.shouldNotify(2500, 1000));
}

void test_cooldown_suppresses_repeat()
{
    SoundDetector d(800, 15000);
    TEST_ASSERT_TRUE(d.shouldNotify(1000, 1000));
    d.notificationSent(1000);
    TEST_ASSERT_FALSE(d.shouldNotify(1000, 1001));
    TEST_ASSERT_FALSE(d.shouldNotify(4000, 15999));
}

void test_notifies_again_after_cooldown()
{
    SoundDetector d(800, 15000);
    d.notificationSent(1000);
    TEST_ASSERT_FALSE(d.shouldNotify(1000, 15999));
    TEST_ASSERT_TRUE(d.shouldNotify(1000, 16000));
}

void test_failed_send_retries_immediately()
{
    SoundDetector d(800, 15000);
    TEST_ASSERT_TRUE(d.shouldNotify(1000, 1000));
    // No notificationSent() -> delivery failed, next loud window fires again.
    TEST_ASSERT_TRUE(d.shouldNotify(1000, 1050));
}

void test_first_event_fires_even_at_time_zero()
{
    SoundDetector d(800, 15000);
    TEST_ASSERT_TRUE(d.shouldNotify(1000, 0));
}

void test_millis_rollover_does_not_freeze_the_cooldown()
{
    SoundDetector d(800, 15000);
    const uint32_t beforeRollover = 0xFFFFFF00u; // 256ms of runtime left
    d.notificationSent(beforeRollover);
    // A cooldown kept as an absolute deadline would have wrapped to a tiny
    // number here and let the next loud window through immediately.
    TEST_ASSERT_FALSE(d.shouldNotify(1000, beforeRollover + 100));
    TEST_ASSERT_FALSE(d.shouldNotify(1000, beforeRollover + 14999)); // past zero
    TEST_ASSERT_TRUE(d.shouldNotify(1000, beforeRollover + 15000));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_below_threshold_never_notifies);
    RUN_TEST(test_at_threshold_notifies);
    RUN_TEST(test_cooldown_suppresses_repeat);
    RUN_TEST(test_notifies_again_after_cooldown);
    RUN_TEST(test_failed_send_retries_immediately);
    RUN_TEST(test_first_event_fires_even_at_time_zero);
    RUN_TEST(test_millis_rollover_does_not_freeze_the_cooldown);
    return UNITY_END();
}
