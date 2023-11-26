#pragma once

#include "defines.h"

// TODO: Disable & enable based on build configuration
#define LOG_WARN_ENABLED
#define LOG_INFO_ENABLED
#define LOG_DEBUG_ENABLED
#define LOG_TRACE_ENABLED

typedef enum log_level {
    LOG_FATAL,
    LOG_ERROR,
    LOG_WARN,
    LOG_INFO,
    LOG_DEBUG,
    LOG_TRACE,
    LOG_LEVEL_MAX
} log_level;

b8 logger_initialize(void);
void logger_shutdown(void);

void log_output(log_level level, const char* message, ...);

// Log messages
#define ETFATAL(message, ...) log_output(LOG_FATAL, message, ##__VA_ARGS__);

#define ETERROR(message, ...) log_output(LOG_ERROR, message, ##__VA_ARGS__);

#ifdef LOG_WARN_ENABLED
#define ETWARN(message, ...) log_output(LOG_WARN, message, ##__VA_ARGS__);
#else
#define ETWARN(message, ...)
#endif

#ifdef LOG_INFO_ENABLED
#define ETINFO(message, ...) log_output(LOG_INFO, message, ##__VA_ARGS__);
#else
#define ETINFO(message, ...)
#endif

#ifdef LOG_DEBUG_ENABLED
#define ETDEBUG(message, ...) log_output(LOG_DEBUG, message, ##__VA_ARGS__);
#else
#define ETDEBUG(message, ...)
#endif

#ifdef LOG_TRACE_ENABLED
#define ETTRACE(message, ...) log_output(LOG_TRACE, message, ##__VA_ARGS__);
#else
#define ETTRACE(message, ...)
#endif