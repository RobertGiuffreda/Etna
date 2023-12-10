#include "etstring.h"

#include "core/etmemory.h"

#include <string.h>

u64 string_length(const char* str) {
    return strlen(str);
}

b8 strings_equal(const char* str0, const char* str1) {
    return strcmp(str0, str1) == 0;
}

char* string_duplicate_allocate(const char* str) {
    u32 len = strlen(str);
    char* str2 = etallocate(sizeof(char) * len + 1, MEMORY_TAG_STRING);
    etcopy_memory(str2, str, sizeof(char) * len);
    str2[len] = '\0';
    return str2;
}

void string_duplicate_free(char* str) {
    etfree(str, sizeof(char) * (strlen(str) + 1), MEMORY_TAG_STRING);
}