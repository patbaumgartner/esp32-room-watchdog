#include <unity.h>

#include <stdio.h>
#include <string.h>

#include "NotificationQueue.h"

void setUp() {}
void tearDown() {}

void test_messages_come_back_in_order()
{
    NotificationQueue<4, 16> queue;
    queue.push("first");
    queue.push("second");

    char message[16];
    TEST_ASSERT_TRUE(queue.pop(message, sizeof(message)));
    TEST_ASSERT_EQUAL_STRING("first", message);
    TEST_ASSERT_TRUE(queue.pop(message, sizeof(message)));
    TEST_ASSERT_EQUAL_STRING("second", message);
    TEST_ASSERT_TRUE(queue.empty());
}

void test_pop_on_empty_queue_reports_failure()
{
    NotificationQueue<2, 16> queue;
    char message[16];
    TEST_ASSERT_FALSE(queue.pop(message, sizeof(message)));
}

void test_full_queue_discards_the_oldest_message()
{
    NotificationQueue<2, 16> queue;
    TEST_ASSERT_TRUE(queue.push("one"));
    TEST_ASSERT_TRUE(queue.push("two"));
    TEST_ASSERT_FALSE(queue.push("three")); // "one" is gone

    char message[16];
    queue.pop(message, sizeof(message));
    TEST_ASSERT_EQUAL_STRING("two", message);
    queue.pop(message, sizeof(message));
    TEST_ASSERT_EQUAL_STRING("three", message);
    TEST_ASSERT_EQUAL_UINT32(1, queue.dropped());
}

void test_size_tracks_pushes_and_pops()
{
    NotificationQueue<3, 16> queue;
    TEST_ASSERT_EQUAL_size_t(0, queue.size());
    queue.push("a");
    queue.push("b");
    TEST_ASSERT_EQUAL_size_t(2, queue.size());

    char message[16];
    queue.pop(message, sizeof(message));
    TEST_ASSERT_EQUAL_size_t(1, queue.size());
}

void test_long_message_is_truncated_not_rejected()
{
    NotificationQueue<2, 8> queue;
    TEST_ASSERT_TRUE(queue.push("0123456789"));

    char message[8];
    TEST_ASSERT_TRUE(queue.pop(message, sizeof(message)));
    TEST_ASSERT_EQUAL_STRING("0123456", message);
}

void test_small_output_buffer_truncates_safely()
{
    NotificationQueue<2, 16> queue;
    queue.push("abcdefgh");

    char message[4];
    TEST_ASSERT_TRUE(queue.pop(message, sizeof(message)));
    TEST_ASSERT_EQUAL_STRING("abc", message);
}

void test_null_message_becomes_empty_string()
{
    NotificationQueue<2, 16> queue;
    TEST_ASSERT_TRUE(queue.push(nullptr));

    char message[16];
    TEST_ASSERT_TRUE(queue.pop(message, sizeof(message)));
    TEST_ASSERT_EQUAL_STRING("", message);
}

void test_ring_wraps_without_corrupting_slots()
{
    NotificationQueue<3, 16> queue;
    char message[16];
    for (int round = 0; round < 10; ++round)
    {
        char pushed[16];
        snprintf(pushed, sizeof(pushed), "msg-%d", round);
        queue.push(pushed);
        TEST_ASSERT_TRUE(queue.pop(message, sizeof(message)));
        TEST_ASSERT_EQUAL_STRING(pushed, message);
    }
    TEST_ASSERT_EQUAL_UINT32(0, queue.dropped());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_messages_come_back_in_order);
    RUN_TEST(test_pop_on_empty_queue_reports_failure);
    RUN_TEST(test_full_queue_discards_the_oldest_message);
    RUN_TEST(test_size_tracks_pushes_and_pops);
    RUN_TEST(test_long_message_is_truncated_not_rejected);
    RUN_TEST(test_small_output_buffer_truncates_safely);
    RUN_TEST(test_null_message_becomes_empty_string);
    RUN_TEST(test_ring_wraps_without_corrupting_slots);
    return UNITY_END();
}
