#include <unity.h>

#include "DistanceTracker.h"

void setUp() {}
void tearDown() {}

void test_zero_distance_never_notifies()
{
    DistanceTracker t(100, 10000);
    TEST_ASSERT_FALSE(t.onDistance(0, 1000));
    TEST_ASSERT_FALSE(t.onDistance(0, 60000));
}

void test_first_distance_sets_baseline_silently()
{
    DistanceTracker t(100, 10000);
    TEST_ASSERT_FALSE(t.onDistance(150, 1000));
}

void test_small_move_stays_quiet()
{
    DistanceTracker t(100, 10000);
    t.onDistance(150, 0);                        // baseline
    TEST_ASSERT_FALSE(t.onDistance(249, 20000)); // 99cm < 100cm delta
}

void test_large_move_notifies_in_both_directions()
{
    DistanceTracker t(100, 10000);
    t.onDistance(300, 0);                       // baseline
    TEST_ASSERT_TRUE(t.onDistance(400, 20000)); // away
    TEST_ASSERT_TRUE(t.onDistance(200, 20000)); // closer
}

void test_min_interval_suppresses_updates()
{
    DistanceTracker t(100, 10000);
    t.onDistance(150, 0); // baseline at t=0
    TEST_ASSERT_FALSE(t.onDistance(400, 9999));
    TEST_ASSERT_TRUE(t.onDistance(400, 10000));
}

void test_sent_update_becomes_new_baseline()
{
    DistanceTracker t(100, 10000);
    t.onDistance(150, 0);
    TEST_ASSERT_TRUE(t.onDistance(300, 20000));
    t.notificationSent(300, 20000);
    TEST_ASSERT_FALSE(t.onDistance(350, 40000)); // 50cm from new baseline
    TEST_ASSERT_TRUE(t.onDistance(450, 40000));  // 150cm from new baseline
}

void test_failed_send_retries()
{
    DistanceTracker t(100, 10000);
    t.onDistance(150, 0);
    TEST_ASSERT_TRUE(t.onDistance(300, 20000));
    // No notificationSent() -> delivery failed, keeps firing.
    TEST_ASSERT_TRUE(t.onDistance(300, 20100));
}

void test_reset_requires_new_baseline()
{
    DistanceTracker t(100, 10000);
    t.onDistance(150, 0);
    t.reset();
    TEST_ASSERT_FALSE(t.onDistance(400, 20000)); // silent baseline again
    TEST_ASSERT_TRUE(t.onDistance(600, 40000));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_zero_distance_never_notifies);
    RUN_TEST(test_first_distance_sets_baseline_silently);
    RUN_TEST(test_small_move_stays_quiet);
    RUN_TEST(test_large_move_notifies_in_both_directions);
    RUN_TEST(test_min_interval_suppresses_updates);
    RUN_TEST(test_sent_update_becomes_new_baseline);
    RUN_TEST(test_failed_send_retries);
    RUN_TEST(test_reset_requires_new_baseline);
    return UNITY_END();
}
