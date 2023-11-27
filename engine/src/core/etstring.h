#pragma once

#include "defines.h"

u64 string_length(const char* str);

b8 strings_equal(const char* str0, const char* str1);

i32 string_format(char* dest, const char* format, ...);

i32 v_string_format(char* dest, const char* format, void* va_list);