#pragma once

#include "defines.h"

typedef enum memory_tag {
    MEMORY_TAG_UNKNOWN,
    MEMORY_TAG_STRING,
    MEMORY_TAG_MAX
} memory_tag;

void memory_init(void);

void* etallocate(u64 size, memory_tag tag);

void etfree(void* block, u64 size, memory_tag tag);

u64 get_total_allocs(void);