#include <unity.h>

#include "PresenceMonitor.h"

using Event = PresenceMonitor::Event;

void setUp() {}
void tearDown() {}

void test_stable_absent_gives_no_event()
{
    PresenceMonitor m(2000);
    TEST_ASSERT_EQUAL(Event::None, m.onSample(false, 0));
    TEST_ASSERT_EQUAL(Event::None, m.onSample(false, 5000));
}

void test_detected_only_after_debounce()
{
    PresenceMonitor m(2000);
    TEST_ASSERT_EQUAL(Event::None, m.onSample(true, 1000)); // edge, debounce starts
    TEST_ASSERT_EQUAL(Event::None, m.onSample(true, 2999)); // still debouncing
    TEST_ASSERT_EQUAL(Event::Detected, m.onSample(true, 3000));
}

void test_flicker_resets_debounce()
{
    PresenceMonitor m(2000);
    m.onSample(true, 1000);
    TEST_ASSERT_EQUAL(Event::None, m.onSample(false, 2000)); // dropped out -> reset
    TEST_ASSERT_EQUAL(Event::None, m.onSample(true, 2100));  // new edge
    TEST_ASSERT_EQUAL(Event::None, m.onSample(true, 4000));  // 1900ms < debounce
    TEST_ASSERT_EQUAL(Event::Detected, m.onSample(true, 4100));
}

void test_confirmed_event_does_not_repeat()
{
    PresenceMonitor m(2000);
    m.onSample(true, 0);
    TEST_ASSERT_EQUAL(Event::Detected, m.onSample(true, 2000));
    m.notificationSent();
    TEST_ASSERT_EQUAL(Event::None, m.onSample(true, 2100));
    TEST_ASSERT_TRUE(m.notifiedState());
}

void test_failed_send_keeps_firing()
{
    PresenceMonitor m(2000);
    m.onSample(true, 0);
    TEST_ASSERT_EQUAL(Event::Detected, m.onSample(true, 2000));
    // notificationSent() not called -> event repeats until delivery succeeds.
    TEST_ASSERT_EQUAL(Event::Detected, m.onSample(true, 2100));
}

void test_cleared_after_detected()
{
    PresenceMonitor m(2000);
    m.onSample(true, 0);
    m.onSample(true, 2000);
    m.notificationSent();

    TEST_ASSERT_EQUAL(Event::None, m.onSample(false, 3000)); // edge
    TEST_ASSERT_EQUAL(Event::None, m.onSample(false, 4999));
    TEST_ASSERT_EQUAL(Event::Cleared, m.onSample(false, 5000));
    m.notificationSent();
    TEST_ASSERT_FALSE(m.notifiedState());
}

void test_millis_rollover_does_not_stall_the_debounce()
{
    PresenceMonitor m(2000);
    const uint32_t beforeRollover = 0xFFFFFF00u; // 256ms of runtime left
    TEST_ASSERT_EQUAL(Event::None, m.onSample(true, beforeRollover));
    // A debounce kept as an absolute deadline would have wrapped to a tiny
    // number here and confirmed presence 2 seconds early.
    TEST_ASSERT_EQUAL(Event::None, m.onSample(true, beforeRollover + 100));
    TEST_ASSERT_EQUAL(Event::None, m.onSample(true, beforeRollover + 1999)); // past zero
    TEST_ASSERT_EQUAL(Event::Detected, m.onSample(true, beforeRollover + 2000));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_stable_absent_gives_no_event);
    RUN_TEST(test_detected_only_after_debounce);
    RUN_TEST(test_flicker_resets_debounce);
    RUN_TEST(test_confirmed_event_does_not_repeat);
    RUN_TEST(test_failed_send_keeps_firing);
    RUN_TEST(test_cleared_after_detected);
    RUN_TEST(test_millis_rollover_does_not_stall_the_debounce);
    return UNITY_END();
}
