#pragma once

#include "defines.h"

u64 string_length(const char* str);

b8 strings_equal(const char* str0, const char* str1);

char* string_duplicate_allocate(const char* str);

void string_duplicate_free(char* str);

void string_format(char* dest, ...);