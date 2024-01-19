#include "logger.h"

#include "core/etmemory.h"
#include "core/etstring.h"

#include <stdarg.h>
#include <stdio.h>

//TEMP:
#include <string.h>
//TEMP: end

// TODO: Linear allocator for this

b8 logger_initialize(void) {
    // TODO: Open log file
    return true;
}

void logger_shutdown(void) {
    // TODO: Close log file
}

// TODO: Use etstring for string manipulation for the sake of it
void log_output(log_level level, const char* format, ...) {
    // TODO: Write message to log file
    const u32 log_str_len = 9;
    const char* log_level_strings[LOG_LEVEL_MAX] = {
        "[FATAL]: ", "[ERROR]: ", "[WARN]:  ", "[INFO]:  ", "[DEBUG]: ", "[TRACE]: ",
    };

    // HACK: Use vsnprintf to get the correct amount of bytes needed to store the string,
    // hacky work arround to avoid parsing the string or adding extraneous bytes
    i32 vsnprintf_len = 0;
    va_list list_len;
    va_start(list_len, format);
    vsnprintf_len = vsnprintf((void*)0, 0, format, list_len);
    va_end(list_len);

    const u32 total_len = vsnprintf_len + log_str_len + 1;
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