#include "etmemory.h"

#include "core/logger.h"

#include <stdio.h>
#include <stdlib.h>
// NOTE: string.h included for memset & memcpy
#include <string.h>

struct memory_metrics {
    u64 total_allocated;
    u64 allocated[MEMORY_TAG_MAX];
};

static struct memory_metrics metrics;

static const char* memory_strings[MEMORY_TAG_MAX] = {
    "Unknown:       ",
    "Engine:        ",
    "Application:   ",
    "Logger:        ",
    "Events:        ",
    "Input:         ",
    "Window:        ",
    "UI:            ",
    "Renderer:      ",
    "Scene:         ",
    "Renderable:    ",
    "Shader:        ",
    "Resource:      ",
    "Material:      ",
    "File System:   ",
    "Dynarray:      ",
    "Descriptor Dyn:",
    "Pipeline Dyn:  ",
    "Material Dyn:  ",
    "String:        "
};
static const u32 mem_tag_str_len = 16;

b8 memory_initialize(void) {
    memset((void*)&metrics, 0, sizeof(struct memory_metrics));
    return true;
}

// Should be zero across the board
void memory_shutdown(void) { }

void* etallocate(u64 size, memory_tag tag) {
    if (tag == MEMORY_TAG_UNKNOWN) {
        ETWARN("Memory allocated with MEMORY_TAG_UNKNOWN.");
    }
    metrics.total_allocated += size; 
    metrics.allocated[tag] += size;
    return malloc(size);
}

void etfree(void* block, u64 size, memory_tag tag) {
    if (tag == MEMORY_TAG_UNKNOWN) {
        ETWARN("Memory freed with MEMORY_TAG_UNKNOWN.");
    }
    metrics.total_allocated -= size;
    metrics.allocated[tag] -= size;
    free(block);
}

void* etzero_memory(void* block, u64 size) {
    return memset(block, 0, size);
}

/* Dest & source can overlapp compared to etcopy_memory. */
void* etmove_memory(void* dest, const void* source, u64 size) {
    return memmove(dest, source, size);
}

void* etcopy_memory(void* dest, const void* source, u64 size) {
    return memcpy(dest, source, size);
}

// TODO: Specific allocation function for memory allocated before memory_intialize() is called
// TODO: Linear allocator for this purpose??
void log_memory_allocations(void) {
    const u64 gib = 1024 * 1024 * 1024;
    const u64 mib = 1024 * 1024;
    const u64 kib = 1024;

    const u8 amount_len = 6;
    const u8 mem_num_len = amount_len + 1 + 2;
    const u8 xib_len = 3;

    // NOTE: (Memory Tag String Length)
    // + (Memory Amount Number String Length)
    // + (xiB (GiB, MiB, KiB) String Length)
    // + 1 (Newline Character)
    u32 output_size = (mem_tag_str_len + mem_num_len + xib_len) + 1;
    
    // Multiplied by the amount of memory tags    
    output_size *= MEMORY_TAG_MAX;

    const char* init = "System memory usage [tagged]: \n";
    u32 init_len = strlen(init);
    output_size += init_len + 1;

    char* output = (char*)malloc(output_size);
    u32 offset = 0;

    etcopy_memory(output, init, init_len);
    offset += init_len;

    for (u32 i = 0; i < MEMORY_TAG_MAX; ++i) {
        char xiB[4] = "XiB";
        f32 amount = 1.0f;
        if (metrics.allocated[i] >= gib) {
            xiB[0] = 'G';
            amount = metrics.allocated[i] / (f32)gib;
        } else if (metrics.allocated[i] >= mib) {
            xiB[0] = 'M';
            amount = metrics.allocated[i] / (f32)mib;
        } else if (metrics.allocated[i] >= kib) {
            xiB[0] = 'K';
            amount = metrics.allocated[i] / (f32)kib;
        } else {
            xiB[0] = 'B';
            xiB[1] = '\0';
            amount = (f32)metrics.allocated[i];
        }
        i32 len = snprintf(output + offset, output_size - offset, "%s%*.2f%s\n", memory_strings[i], mem_num_len, amount, xiB);
        offset += len;
    }
    ETDEBUG(output);
    free(output);
}