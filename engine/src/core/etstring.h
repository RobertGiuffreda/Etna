#pragma once

#include "defines.h"

// TODO: Implement own string concept (maybe) struct string {...} 

u64 str_length(const char* str);

b8 strs_equal(const char* str0, const char* str1);

b8 strsn_equal(const char* str0, const char* str1, u64 n);

char* strn_copy(char* dst, const char* src, u64 n);

char* str_copy(char* dst, const char* src);

char* str_char_search(const char* str, int c);

char* str_str_search(const char* str, const char* sub_str);

char* str_duplicate_allocate(const char* str);

// TODO: Calls strlen to free this mem which is bad
void str_duplicate_free(char* str);