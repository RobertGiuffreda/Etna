#pragma once
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float f32;
typedef double f64;

typedef int32_t b32;
typedef _Bool b8;

// Properly define static assertions
#if defined(__GNUC__)
    #define STATIC_ASSERT _Static_assert
#else
    #define STATIC_ASSERT static_assert
#endif

STATIC_ASSERT(sizeof(float) == 4, "Unsupported architecture, float should be 32 bits.");
STATIC_ASSERT(sizeof(double) == 8, "Unsupported architecture, double should be 64 bits.");

#define true 1
#define false 0

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
    #define ET_WINDOWS 1
    #if !defined(_WIN64)
        #error "64-bit required on Windows."
    #endif
#elif defined(__linux__) || defined(__gnu_linux__)
    #define ET_LINUX 1
    #if defined(__ANDROID__)
        #define ET_ANDROID 1
    #endif
#elif defined(__unix__)
    #define ET_UNIX 1
#elif defined(_POSIX_VERSION)
    #define ET_POSIX 1
#else
    #error "Unknown Platform"
#endif

#if defined(_DEBUG)
    #define DEBUG_BLOCK(...) __VA_ARGS__
#else
    #define DEBUG_BLOCK(...)
#endif

#define BLOCK(...)  \
do {                \
    __VA_ARGS__     \
} while(0);         \

// NOTE: Pass code in BLOCK macro.
#if defined(_DEBUG)
    #define DEBUG_RELEASE(debug, release) debug
#else
    #define DEBUG_RELEASE(debug, release) release
#endif