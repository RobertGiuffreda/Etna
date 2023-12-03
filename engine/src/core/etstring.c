#include "etstring.h"

#include <string.h>

u64 string_length(const char* str) {
    return strlen(str);
}

b8 strings_equal(const char* str0, const char* str1) {
    return strcmp(str0, str1) == 0;
}