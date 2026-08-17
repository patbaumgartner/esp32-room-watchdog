#pragma once

#include <stddef.h>
#include <stdint.h>

// Comparison of the LAN API's shared secret. Lives here rather than in api.cpp
// so the firmware's one security-critical decision has native test coverage.
//
// Pure logic: no hardware deps.
class ApiToken
{
public:
    // Bytes to skip if the header value carries a "Bearer " prefix, else 0.
    static size_t bearerPrefixLength(const char *header)
    {
        static const char PREFIX[] = "Bearer ";
        for (size_t i = 0; i + 1 < sizeof(PREFIX); ++i)
        {
            if (header[i] != PREFIX[i])
            {
                return 0; // a short header hits its terminator here first
            }
        }
        return sizeof(PREFIX) - 1;
    }

    // Always touches every byte of the expected token, so the time a rejection
    // takes does not reveal how many leading bytes of a guess were right.
    // An empty expected token matches nothing.
    static bool matches(const char *supplied, size_t suppliedLength,
                        const char *expected, size_t expectedLength)
    {
        uint8_t difference = suppliedLength == expectedLength ? 0 : 1;
        for (size_t i = 0; i < expectedLength; ++i)
        {
            const char candidate = i < suppliedLength ? supplied[i] : '\0';
            difference |= static_cast<uint8_t>(candidate ^ expected[i]);
        }
        return difference == 0 && expectedLength > 0;
    }
};
