#pragma once

#include "defines.h"

typedef enum memory_tag {
    MEMORY_TAG_ENGINE,
    MEMORY_TAG_APPLICATION,
    MEMORY_TAG_LOGGER,
    MEMORY_TAG_EVENTS,
    MEMORY_TAG_INPUT,
    MEMORY_TAG_WINDOW,
    MEMORY_TAG_SWAPCHAIN,
    MEMORY_TAG_RENDERER,
    MEMORY_TAG_SCENE,
    MEMORY_TAG_SHADER,
    MEMORY_TAG_IMPORTER,
    MEMORY_TAG_RESOURCE,
    MEMORY_TAG_TRANSFORM,
    MEMORY_TAG_FILE,
    MEMORY_TAG_DYNARRAY,
    MEMORY_TAG_STRING,
    MEMORY_TAG_MAX
} memory_tag;

b8 memory_initialize(void);

void memory_shutdown(void);

void print_memory_metrics(void);

void* etallocate(u64 size, memory_tag tag);

void etfree(void* block, u64 size, memory_tag tag);

void* etset_memory(void* block, u8 value, u64 size);
void* etone_memory(void* block, u64 size);
void* etzero_memory(void* block, u64 size);

void* etcopy_memory(void* dest, const void* source, u64 size);

void* etmove_memory(void* dest, const void* source, u64 size);