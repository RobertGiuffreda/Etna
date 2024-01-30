#include "logger.h"

#include "core/etmemory.h"
#include "core/etstring.h"
#include "core/etfile.h"

#include "platform/platform.h"

#include <stdarg.h>
#include <stdio.h>

#include <core/etstring.h>

//TEMP: String stuff
#include <string.h>
//TEMP: end

// ANSI Escape sequences
#define _ESC "\x1B"
#define _CSI "\x1B["
#define _OSC "\x1B]"
#define _ST "\x1b\\"

// Resets all colors and styles
#define _ALL_RESET _CSI "0m"

#define _FOREGROUND "38;"
#define _BACKGROUND "48;"

#define _ERASE_LINE _CSI "K"

#define _COLOR_8BIT(_col) "5;" _col "m"
#define _COLOR_RGB(_r, _g, _b) "2;" _r ";" _g ";" _b "m"

#define _FG_8BIT(_value) _CSI _FOREGROUND _COLOR_8BIT(#_value)
#define _BG_8BIT(_value) _CSI _BACKGROUND _COLOR_8BIT(#_value)

#define _FG_RGB(_r, _g, _b) _CSI _FOREGROUND _COLOR_RGB(#_r, #_g, #_b)
#define _BG_RGB(_r, _g, _b) _CSI _BACKGROUND _COLOR_RGB(#_r, #_g, #_b)

#define _FG_RESET _CSI "39m"
#define _BG_RESET _CSI "49m"

#define _BOLD _CSI "1m"
#define _UNDERLINE _CSI "4m"

#define _BOLD_RESET _CSI "22m"
#define _UNDERLINE_RESET _CSI "24m"

#define _ALTERNATE_BUFFER _CSI "?1049h"
#define _MAIN_BUFFER _CSI "?1049l"

#define ANSI_FATAL_PREFIX _FG_RGB(255, 0, 30) _UNDERLINE _BOLD
#define ANSI_FATAL_POSTFIX _BOLD_RESET _UNDERLINE_RESET _FG_RESET

#define ANSI_ERROR_PREFIX _FG_RGB(255, 77, 10) _BOLD
#define ANSI_ERROR_POSTFIX _BOLD_RESET _FG_RESET

#define ANSI_WARN_PREFIX _FG_RGB(255, 215, 0) _BOLD
#define ANSI_WARN_POSTFIX _BOLD_RESET _FG_RESET

#define ANSI_INFO_PREFIX _FG_RGB(63, 90, 54)
#define ANSI_INFO_POSTFIX _FG_RESET

#define ANSI_DEBUG_PREFIX _FG_RGB(150, 30, 150)
#define ANSI_DEBUG_POSTFIX _FG_RESET

#define ANSI_TRACE_PREFIX _FG_RGB(20, 40, 130)
#define ANSI_TRACE_POSTFIX _FG_RESET

#define ETNA_BACKGROUND(r_hex, g_hex, b_hex) _OSC "10;16;rgb:" #r_hex "/" #g_hex "/" #b_hex _ST

struct logger_state {
    etfile* log_file;
    etfile* err_file;
};

static struct logger_state* logger;

static char* ansi_prefix[LOG_LEVEL_MAX];
static char* ansi_postfix[LOG_LEVEL_MAX];

b8 logger_initialize(void) {
    b8 ansi = platform_init_ANSI_escape();
    ansi_prefix[LOG_FATAL]  = (ansi) ? (ANSI_FATAL_PREFIX "[FATAL]: ")  : "[FATAL]: ";
    ansi_prefix[LOG_ERROR]  = (ansi) ? (ANSI_ERROR_PREFIX "[ERROR]: ")  : "[ERROR]: ";
    ansi_prefix[LOG_WARN]   = (ansi) ? (ANSI_WARN_PREFIX "[WARN]:  ")   : "[WARN]:  ";
    ansi_prefix[LOG_INFO]   = (ansi) ? (ANSI_INFO_PREFIX "[INFO]:  ")   : "[INFO]:  ";
    ansi_prefix[LOG_DEBUG]  = (ansi) ? (ANSI_DEBUG_PREFIX "[DEBUG]: ")  : "[DEBUG]: ";
    ansi_prefix[LOG_TRACE]  = (ansi) ? (ANSI_TRACE_PREFIX "[TRACE]: ")  : "[TRACE]: ";

    ansi_postfix[LOG_TRACE] = (ansi) ? (ANSI_TRACE_POSTFIX) : "";
    ansi_postfix[LOG_ERROR] = (ansi) ? (ANSI_ERROR_POSTFIX) : "";
    ansi_postfix[LOG_WARN]  = (ansi) ? (ANSI_WARN_POSTFIX)  : "";
    ansi_postfix[LOG_INFO]  = (ansi) ? (ANSI_INFO_POSTFIX)  : "";
    ansi_postfix[LOG_DEBUG] = (ansi) ? (ANSI_DEBUG_POSTFIX) : "";
    ansi_postfix[LOG_FATAL] = (ansi) ? (ANSI_FATAL_POSTFIX) : "";
    if (ansi) printf(ETNA_BACKGROUND(05, 05, 14));

    logger = etallocate(sizeof(logger_state), MEMORY_TAG_LOGGER);
    if (!file_open("etna_log.txt", FILE_WRITE_FLAG, &logger->log_file)) {
        logger->log_file = 0;
        ETERROR("Unable to open log file for writing.");
    }
    if (!file_open("etna_err.txt", FILE_WRITE_FLAG, &logger->err_file)) {
        logger->err_file = 0;
        ETERROR("Unable to intialize error file for writing.");
    }

    return true;
}

void logger_shutdown(void) {
    if (logger->log_file) file_close(logger->log_file);
    if (logger->err_file) file_close(logger->err_file);
    etfree(logger, sizeof(logger_state), MEMORY_TAG_LOGGER);
}

// TODO: Use etstring for string manipulation for the sake of it
void log_output(log_level level, const char* format, ...) {
    u32 ansi_prefix_len = str_length(ansi_prefix[level]);
    u32 ansi_postfix_len = str_length(ansi_postfix[level]);

    // HACK: Use vsnprintf to get the correct amount of bytes needed to store the string,
    // hacky work arround to avoid parsing the string or adding extraneous bytes
    i32 format_len = 0;
    va_list list_len;
    va_start(list_len, format);
    format_len = vsnprintf(NULL, 0, format, list_len);
    va_end(list_len);

    const u32 output_len = ansi_prefix_len + format_len + ansi_postfix_len;
    char* output = etallocate(sizeof(char) * (output_len + 1), MEMORY_TAG_STRING);

    // Add one to postfix length to copy the null terminating character
    etcopy_memory(output, ansi_prefix[level], ansi_prefix_len);
    va_list list;
    va_start(list, format);
    vsnprintf(output + ansi_prefix_len, format_len + 1, format, list);
    va_end(list);
    etcopy_memory((u8*)output + ansi_prefix_len + format_len, ansi_postfix[level], ansi_postfix_len + 1);

    // Standard output
    printf("%s\n", output);

    // TODO: If log file fails to initialize then just dont write to log file
    // Change the null terminator to a newline character so that it is printed to the log file.
    output[output_len] = '\n';
    u64 log_bytes_written = 0;
    if (logger->log_file && !file_write(logger->log_file, sizeof(char) * (format_len + 1), (u8*)output + ansi_prefix_len, &log_bytes_written)) {
        printf("Something went wrong when writing to the log file.");
    }
    u64 err_bytes_written = 0;
    if (level < 2 && logger->err_file && !file_write(logger->err_file, sizeof(char) * (format_len + 1), (u8*)output + ansi_prefix_len, &err_bytes_written)) {
        printf("Something went wrong when writing to the error file.");
    }

    etfree(output, sizeof(char) * (output_len + 1), MEMORY_TAG_STRING);
}

void log_assert_failure(const char* expression, const char* message, const char* file, i32 line) {
    log_output(LOG_FATAL, "Assertion Failure: %s, message: '%s', in file: %s, line %d\n", expression, message, file, line);
}