#pragma once
#include <core/logger.h>

#include <math.h>

#define expect_should_be(expected, actual)                                                                  \
    do {                                                                                                    \
        if (actual != expected) {                                                                           \
            ETERROR("--> Expected %lld, but got: %lld. File %s:%d.", expected, actual, __FILE__, __LINE__); \
            return false;                                                                                   \
        }                                                                                                   \
    } while(0);                                                                                             \

#define expect_should_not_be(expected, actual)                                                                              \
    do {                                                                                                                    \
        if (actual == expected) {                                                                                           \
            ETERROR("--> Expected %lld != %lld, but they are equal. File %s:%d.", expected, actual, __FILE__, __LINE__);    \
            return false;                                                                                                   \
        }                                                                                                                   \
    } while(0);                                                                                                             \

#define expect_float_to_be(expected, actual)                                                                \
    do {                                                                                                    \
        if (abs(expected - actual) > 0.001f) {                                                            \
            ETERROR("--> Expected %f, but got: %f. File %s:%d.", expected, actual, __FILE__, __LINE__);     \
            return false;                                                                                   \
        }                                                                                                   \
    } while(0);                                                                                             \

#define expect_to_be_true(actual)                                                                               \
    do {                                                                                                        \
        if (actual != true) {                                                                                   \
            ETERROR("--> Expected %llu to be true, but it is false. File %s:%d.", actual, __FILE__, __LINE__);  \
            return false;                                                                                       \
        }                                                                                                       \
    } while(0);                                                                                                 \

#define expect_to_be_false(actual)                                                                              \
    do {                                                                                                        \
        if (actual != false) {                                                                                  \
            ETERROR("--> Expected %llu to be false, but it is true. File %s:%d.", actual, __FILE__, __LINE__);  \
            return false;                                                                                       \
        }                                                                                                       \
    } while(0);                                                                                                 \

