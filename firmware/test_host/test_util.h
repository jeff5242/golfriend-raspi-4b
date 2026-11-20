#pragma once
/* 主機端（macOS／Linux）用的極簡測試巨集，不依賴 ESP-IDF。 */
#include <stdio.h>
#include <string.h>

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        g_checks++;                                                          \
        if (!(cond)) {                                                       \
            g_failures++;                                                    \
            fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                    \
    } while (0)

#define CHECK_STR(actual, expected)                                          \
    do {                                                                     \
        g_checks++;                                                          \
        if (strcmp((actual), (expected)) != 0) {                             \
            g_failures++;                                                    \
            fprintf(stderr, "  FAIL %s:%d: got \"%s\", want \"%s\"\n",       \
                    __FILE__, __LINE__, (actual), (expected));               \
        }                                                                    \
    } while (0)

#define RUN(test_fn)                    \
    do {                                \
        printf("- %s\n", #test_fn);     \
        test_fn();                      \
    } while (0)

#define TEST_SUMMARY()                                                     \
    (printf("%s: %d checks, %d failures\n", __FILE__, g_checks, g_failures), \
     g_failures == 0 ? 0 : 1)
