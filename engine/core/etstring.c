#include "etstring.h"

#include "memory/etmemory.h"
#include "core/asserts.h"

#include <string.h>

// TODO: Create etstring that is a fat pointer
// typedef struct {
//     size_t size;
//     char* string;
// } etstring;

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

char* sub_strs_concat_alloc(const char* str0, u32 num0, const char* str1, u32 num1) {
    ETASSERT(num0 <= str_length(str0));
    ETASSERT(num1 <= str_length(str1));
    
    char* concat = str_alloc(num0 + num1);
    etcopy_memory(concat, str0, num0);
    etcopy_memory(concat + num0, str1, num1);
    return concat;
}

u32 str_char_offset(const char* str, const char* c) {
    // ASSERT the char is within the string
    ETASSERT_DEBUG((u64)c > (u64)str && (u64)c < ((u64)str + strlen(str)));
    return (u64)c - (u64)str;
}

char* str_dup_alloc(const char* str) {
    u32 len = strlen(str);
    char* alloc = str_alloc(len);
    etcopy_memory(alloc, str, sizeof(char) * len);
    return alloc;
}

char* str_alloc(u64 size) {
    char* alloc = etallocate(sizeof(char) * (size + 1), MEMORY_TAG_STRING);
    alloc[size] = '\0';
    return alloc;
}

void str_free(char* str) {
    etfree(str, sizeof(char) * (strlen(str) + 1), MEMORY_TAG_STRING);
}