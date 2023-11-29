#pragma once

#include "defines.h"

typedef enum memory_tag {
    MEMORY_TAG_UNKNOWN,
    MEMORY_TAG_ENGINE,
    MEMORY_TAG_APPLICATION,
    MEMORY_TAG_EVENTS,
    MEMORY_TAG_INPUT,
    MEMORY_TAG_WINDOW,
    MEMORY_TAG_FILESYSTEM,
    MEMORY_TAG_DYNARRAY,
    MEMORY_TAG_STRING,
    MEMORY_TAG_MAX
} memory_tag;

b8 memory_initialize(void);

void memory_shutdown(void);

void* etallocate(u64 size, memory_tag tag);

void etfree(void* block, u64 size, memory_tag tag);

void* etzero_memory(void* block, u64 size);

void* etmove_memory(void* dest, const void* source, u64 size);

void* etcopy_memory(void* dest, const void* source, u64 size);

u64 get_total_allocs(void);