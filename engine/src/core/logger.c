#include "logger.h"
#include "etmemory.h"

#include <stdarg.h>
#include <stdio.h>

//TEMP:
#include <string.h>
//TEMP: end

// TODO: Linear allocator for this

b8 logger_initialize(void) {
    ETFATAL("This is a FATAL message.");
    ETERROR("This is a ERROR message.");
    ETWARN("This is a WARN message.");
    ETINFO("This is a INFO message.");
    ETDEBUG("This is a DEBUG message.");
    ETTRACE("This is a TRACE message.");
    return true;
}

void logger_shutdown(void) {}

// TODO: Linear allocator for this
// TODO: Parse the va_list correctly
void log_output(log_level level, const char* format, ...) {
    const char* log_level_strings[LOG_LEVEL_MAX] = {
        "[FATAL]: ", "[ERROR]: ", "[WARN]:  ", "[INFO]:  ", "[DEBUG]: ", "[TRACE]: ",
    };
    
    const u8 log_str_len = 9;
    const u32 format_len = strlen(format);
    const u32 total_len = log_str_len + format_len + 1 /* To avoid parsing string to get true length: */ + 1024;
    // 1 added for null terminator
    char* output = etallocate(sizeof(char) * total_len, MEMORY_TAG_STRING);

    // NOTE: Could be a memcpy
    sprintf(output, "%s", log_level_strings[level]);
    
    va_list list;
    va_start(list, format);
    vsnprintf(output + log_str_len, total_len - log_str_len, format, list);
    va_end(list);

    printf("%s\n", output);
    etfree(output, sizeof(char) * total_len, MEMORY_TAG_STRING);
}

void log_assert_failure(const char* expression, const char* message, const char* file, i32 line) {
    log_output(LOG_FATAL, "Assertion Failure: %s, message: '%s', in file: %s, line %d\n", expression, message, file, line);
}