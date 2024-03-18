#include "etstring.h"

#include "memory/etmemory.h"

#include <string.h>


u64 str_length(const char* str) {
    return strlen(str);
}

b8 strs_equal(const char* str0, const char* str1) {
    return strcmp(str0, str1) == 0;
}

b8 strsn_equal(const char* str0, const char* str1, u64 n) {
    return strncmp(str0, str1, n) == 0;
}

char* strn_copy(char* dst, const char* src, u64 n) {
    return strncpy(dst, src, n);
}

char* str_copy(char* dst, const char* src) {
    return strcpy(dst, src);
}

char* str_char_search(const char* str, int c) {
    return strchr(str, c);
}

char* str_str_search(const char* str, const char* sub_str) {
    return strstr(str, sub_str);
}

char* rev_str_char_search(const char* str, char c) {
    u32 str_len = strlen(str);
    while (--str_len >= 0 && str[str_len] != c);
    if (str_len < 0) return NULL;

    return (char *)&str[str_len];
}

char* str_duplicate_allocate(const char* str) {
    u32 len = strlen(str);
    char* str2 = etallocate(sizeof(char) * len + 1, MEMORY_TAG_STRING);
    etcopy_memory(str2, str, sizeof(char) * len);
    str2[len] = '\0';
    return str2;
}

void str_duplicate_free(char* str) {
    etfree(str, sizeof(char) * (strlen(str) + 1), MEMORY_TAG_STRING);
}