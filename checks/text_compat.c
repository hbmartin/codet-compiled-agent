/* Host-only equivalence checks. Links no recovered application code. */
#include "text_compat.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    char source[96], expected[128], actual[128];
    unsigned cases = 0;
    for (size_t length = 0; length < sizeof source; ++length) {
        memset(source, 'x', sizeof source);
        for (size_t i = 0; i < length; ++i) source[i] = (char)(1 + i * 7 % 255);
        source[length] = '\0';
        for (size_t count = 0; count <= sizeof source; ++count) {
            memset(expected, 0x5a, sizeof expected);
            memset(actual, 0x5a, sizeof actual);
            assert(strncpy(expected + 8, source, count) == expected + 8);
            assert(app_copy_padded(actual + 8, source, count) == actual + 8);
            assert(memcmp(expected, actual, sizeof actual) == 0);
            ++cases;
        }
    }
    for (size_t capacity = 0; capacity <= 96; ++capacity) {
        int expected_count = -1, actual_count = -1;
        memset(expected, 0x5a, sizeof expected);
        memset(actual, 0x5a, sizeof actual);
        int a = snprintf(expected + 8, capacity, "%s/%d/%08x/%.3f/%%/%n",
                         "a string longer than small output fields", -42, 0x1234u,
                         1.234567, &expected_count);
        int b = app_format_truncated(actual + 8, capacity, "%s/%d/%08x/%.3f/%%/%n",
                                     "a string longer than small output fields", -42,
                                     0x1234u, 1.234567, &actual_count);
        assert(a == b && expected_count == actual_count);
        assert(memcmp(expected, actual, sizeof actual) == 0);
        ++cases;
    }
    assert(app_format_truncated(NULL, 0, "%s:%d", "length", 42) ==
           snprintf(NULL, 0, "%s:%d", "length", 42));
    printf("PASS: %u host-only text equivalence cases; no application code linked or run.\n", cases + 1);
    return 0;
}
