#include <unity.h>

#include <string.h>

#include "ApiToken.h"

void setUp() {}
void tearDown() {}

static const char EXPECTED[] = "s3cret-token-value";

static bool matches(const char *supplied)
{
    return ApiToken::matches(supplied, strlen(supplied), EXPECTED, strlen(EXPECTED));
}

void test_exact_token_matches()
{
    TEST_ASSERT_TRUE(matches(EXPECTED));
}

void test_wrong_token_of_equal_length_is_rejected()
{
    TEST_ASSERT_FALSE(matches("s3cret-token-valuX"));
}

void test_correct_prefix_is_not_enough()
{
    TEST_ASSERT_FALSE(matches("s3cret"));
}

void test_token_with_trailing_bytes_is_rejected()
{
    TEST_ASSERT_FALSE(matches("s3cret-token-value-and-more"));
}

void test_empty_supplied_token_is_rejected()
{
    TEST_ASSERT_FALSE(matches(""));
}

void test_empty_expected_token_matches_nothing()
{
    TEST_ASSERT_FALSE(ApiToken::matches("", 0, "", 0));
    TEST_ASSERT_FALSE(ApiToken::matches("anything", 8, "", 0));
}

void test_embedded_nul_does_not_truncate_the_comparison()
{
    // A supplied value that is "s3cret\0..." must not pass just because a
    // C-string comparison would stop at the NUL.
    char supplied[sizeof(EXPECTED)];
    memcpy(supplied, EXPECTED, sizeof(EXPECTED));
    supplied[6] = '\0';
    TEST_ASSERT_FALSE(ApiToken::matches(supplied, strlen(EXPECTED), EXPECTED, strlen(EXPECTED)));
}

void test_bearer_prefix_is_recognized()
{
    TEST_ASSERT_EQUAL_size_t(7, ApiToken::bearerPrefixLength("Bearer abc"));
}

void test_other_authorization_schemes_are_not_stripped()
{
    TEST_ASSERT_EQUAL_size_t(0, ApiToken::bearerPrefixLength("Basic abc"));
    TEST_ASSERT_EQUAL_size_t(0, ApiToken::bearerPrefixLength("bearer abc")); // case sensitive
    TEST_ASSERT_EQUAL_size_t(0, ApiToken::bearerPrefixLength("Bearer"));     // no separator
    TEST_ASSERT_EQUAL_size_t(0, ApiToken::bearerPrefixLength(""));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_exact_token_matches);
    RUN_TEST(test_wrong_token_of_equal_length_is_rejected);
    RUN_TEST(test_correct_prefix_is_not_enough);
    RUN_TEST(test_token_with_trailing_bytes_is_rejected);
    RUN_TEST(test_empty_supplied_token_is_rejected);
    RUN_TEST(test_empty_expected_token_matches_nothing);
    RUN_TEST(test_embedded_nul_does_not_truncate_the_comparison);
    RUN_TEST(test_bearer_prefix_is_recognized);
    RUN_TEST(test_other_authorization_schemes_are_not_stripped);
    return UNITY_END();
}
