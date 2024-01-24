#pragma once
#include <stdint.h>
#include <stdalign.h>

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
#if defined(__clang__) || defined(__gcc__)
#define STATIC_ASSERT _Static_assert
#else
#define STATIC_ASSERT static_assert
#endif

// Ensure types are the correct amount of bytes
STATIC_ASSERT(sizeof(u8) == 1, "Expected u8 to be 1 byte.");
STATIC_ASSERT(sizeof(u16) == 2, "Expected u16 to be 2 bytes.");
STATIC_ASSERT(sizeof(u32) == 4, "Expected u32 to be 4 bytes.");
STATIC_ASSERT(sizeof(u64) == 8, "Expected u64 to be 8 bytes.");

STATIC_ASSERT(sizeof(i8) == 1, "Expected i8 to be 1 byte.");
STATIC_ASSERT(sizeof(i16) == 2, "Expected i16 to be 2 bytes.");
STATIC_ASSERT(sizeof(i32) == 4, "Expected i32 to be 4 bytes.");
STATIC_ASSERT(sizeof(i64) == 8, "Expected i64 to be 8 bytes.");

STATIC_ASSERT(sizeof(f32) == 4, "Expected f32 to be 4 bytes.");
STATIC_ASSERT(sizeof(u64) == 8, "Expected f64 to be 8 bytes.");

STATIC_ASSERT(sizeof(b32) == 4, "Expected b32 to be 4 byte.");
STATIC_ASSERT(sizeof(b8) == 1, "Expected b8 to be 1 byte.");

#define true 1
#define false 0

// TODO: More readable Platform 
// Platform detection
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
#define ETPLATFORM_WINDOWS 1
#ifndef _WIN64
#error "64-bit required on Windows."
#endif
#elif defined(__linux__) || defined(__gnu_linux__)
// Linux OS
#define ETPLATFORM_LINUX 1
#if defined(__ANDROID__)
#define ETPLATFORM_ANDROID 1
#endif
#elif defined(__unix__)
#define ETPLATFORM_UNIX 1
#elif defined(_POSIX_VERSION)
//Posix
#define ETPLATFORM_POSIX 1
#elif __APPLE__
//Apple platform
#define ETPLATFORM_APPLE 1
//Apple Platform
#include <TargetConditionals.h>
#if TARGET_IPHONE_SIMULATOR
//ios simulator
#define ETPLATFORM_IOS 1
#define ETPLATFORM_IOS_SIMULATOR 1
#elif TARGET_OS_IPHONE
#define ETPLATFORM_IOS 1
//ios device
#elif TARGET_OS_MAC
//other mac os variants
#else
#error "Unknown Apple platform"
#endif
#else
#error "Unknown Platform"
#endif
