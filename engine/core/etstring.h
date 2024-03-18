#pragma once

#include "defines.h"

// NOTE: Still uses null terminator for literals
typedef struct etstring {
    u64 length;
    u8* str;
} etstring;

#define CONCAT(a, b) a ## b
#define CONCAT2(a, b) CONCAT(a, b)

// etstring name = etstring_lit("literal string")
#define etstring_lit(literal) {     \
    .length = sizeof(literal) - 1,  \
    .str = literal,                 \
}                                   \

// So str pointer can be modified, unlike a string literal.
#define etstring_rval(name, literal) \
u8 CONCAT2(_tmp, __LINE__)[] = literal; etstring name = { .length = sizeof(literal) - 1, .str = CONCAT2(_tmp, __LINE__), };


u64 str_length(const char* str);

b8 strs_equal(const char* str0, const char* str1);

b8 strsn_equal(const char* str0, const char* str1, u64 n);

char* strn_copy(char* dst, const char* src, u64 n);

char* str_copy(char* dst, const char* src);

char* str_char_search(const char* str, int c);

char* str_str_search(const char* str, const char* sub_str);

char* rev_str_char_search(const char* str, char c);

char* str_duplicate_allocate(const char* str);

// TODO: Calls strlen to free this mem which is bad
void str_duplicate_free(char* str);