#include "logger.h"

#include "core/etmemory.h"
#include "core/etstring.h"
#include "core/etfile.h"

#include <stdarg.h>
#include <stdio.h>

//TEMP:
#include <string.h>
//TEMP: end

// TODO: Linear allocator for this

struct logger_state {
    etfile* log_file;
};

static struct logger_state* logger;

b8 logger_initialize(void) {
    logger = etallocate(sizeof(logger_state), MEMORY_TAG_LOGGER);
    if (!file_open("etna_log.txt", FILE_WRITE_FLAG, &logger->log_file)) {
        ETERROR("Unable to open log file for writting.");
        return false;
    }
    return true;
}

void logger_shutdown(void) {
    if (logger->log_file) file_close(logger->log_file);
    etfree(logger, sizeof(logger_state), MEMORY_TAG_LOGGER);
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

    // Change the null terminator to a newline character so that it is printed to the log file.
    output[total_len - 1] = '\n';
    u64 log_bytes_written = 0;
    if (!file_write(logger->log_file, sizeof(char) * total_len, output, &log_bytes_written)) {
        ETWARN("Something went wrong when writing to the log file.");
    }
    
    etfree(output, sizeof(char) * total_len, MEMORY_TAG_STRING);
}

void log_assert_failure(const char* expression, const char* message, const char* file, i32 line) {
    log_output(LOG_FATAL, "Assertion Failure: %s, message: '%s', in file: %s, line %d\n", expression, message, file, line);
}