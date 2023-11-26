#pragma once

#include "defines.h"

#define ASSERTIONS_ENABLED

#ifdef ASSERTIONS_ENABLED

#if _MSC_VER
#include <intrin.h>
#define DEBUGBREAK() __debugbreak()
#else //_MSC_VER
#define DEBUGBREAK() __builtin_trap()
#endif //_MSC_VER

void log_assert_failure(const char* expression, const char* message, const char* file, i32 line);

#define ETASSERT(expr) { if(!(expr)) { log_assert_failure(#expr, "", __FILE__, __LINE__); DEBUGBREAK(); } }
#define ETASSERT_MESSAGE(expr, message) { if (!(expr)) { log_assert_failure(#expr, message, __FILE__, __LINE__); DEBUGBREAK(); } }

#ifdef _DEBUG
#define ETASSERT_DEBUG(expr) { if(!(expr)) { log_assert_failure(#expr, "", __FILE__, __LINE__); DEBUGBREAK(); } }
#else // _DEBUG
#define ETASSERT_DEBUG(expr)
#endif // _DEBUG

#else //ASSERTIONS_ENABLED 
#define ETASSERT(expr)
#define ETASSERT_MSG(expr, message)
#define ETASSERT_DEBUG(expr)
#endif //ASSERTIONS_ENABLED