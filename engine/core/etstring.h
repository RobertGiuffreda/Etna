#pragma once

#include "defines.h"

u64 str_length(const char* str);

b8 strs_equal(const char* str0, const char* str1);

b8 strsn_equal(const char* str0, const char* str1, u64 n);

char* strn_copy(char* dst, const char* src, u64 n);

char* str_copy(char* dst, const char* src);

char* str_char_search(const char* str, int c);

char* str_str_search(const char* str, const char* sub_str);

char* rev_str_char_search(const char* str, char c);

char* sub_strs_concat_alloc(const char* str0, u32 num0, const char* str1, u32 num1);

u32 str_char_offset(const char* str, const char* c);

char* str_alloc(u64 size);

void str_free(char* str);

char* str_dup_alloc(const char* str);