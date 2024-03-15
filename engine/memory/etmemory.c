#include "etmemory.h"

#include "core/logger.h"

#include <stdio.h>
#include <stdlib.h>
// NOTE: string.h included for memset & memcpy
#include <string.h>

typedef struct memory_metric {
    u64 allocated;
    u64 allocations;
} memory_metric;

struct memory_metrics {
    memory_metric total_metrics;
    memory_metric tag_metrics[MEMORY_TAG_MAX];
};

static struct memory_metrics metrics;

static const char* memory_strings[MEMORY_TAG_MAX] = {
    "Engine:        ",
    "Application:   ",
    "Logger:        ",
    "Events:        ",
    "Input:         ",
    "Window:        ",
    "Swapchain:     ",
    "Renderer:      ",
    "Scene:         ",
    "Shader:        ",
    "Importer:      ",
    "Resource:      ",
    "File:          ",
    "Dynarray:      ",
    "String:        "
};
static const u32 mem_tag_str_len = 16;

b8 memory_initialize(void) {
    memset((void*)&metrics, 0, sizeof(struct memory_metrics));
    return true;
}

void memory_shutdown(void) { }

void* etallocate(u64 size, memory_tag tag) {
    metrics.total_metrics.allocated += size;
    metrics.total_metrics.allocations++;
    
    metrics.tag_metrics[tag].allocated += size;
    metrics.tag_metrics[tag].allocations++;
    return malloc(size);
}

void etfree(void* block, u64 size, memory_tag tag) {
    metrics.total_metrics.allocated -= size;
    metrics.total_metrics.allocations--;

    metrics.tag_metrics[tag].allocated -= size;
    metrics.tag_metrics[tag].allocations--;
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
void print_memory_metrics(void) {
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
        memory_metric tag_metric = metrics.tag_metrics[i];

        char xiB[4] = "XiB";
        f32 amount = 1.0f;
        if (tag_metric.allocated >= gib) {
            xiB[0] = 'G';
            amount = tag_metric.allocated / (f32)gib;
        } else if (tag_metric.allocated >= mib) {
            xiB[0] = 'M';
            amount = tag_metric.allocated / (f32)mib;
        } else if (tag_metric.allocated >= kib) {
            xiB[0] = 'K';
            amount = tag_metric.allocated / (f32)kib;
        } else {
            xiB[0] = 'B';
            xiB[1] = '\0';
            amount = (f32)tag_metric.allocated;
        }
        i32 len = snprintf(output + offset, output_size - offset, "%s%*.2f%s\n", memory_strings[i], mem_num_len, amount, xiB);
        offset += len;
    }
    ETDEBUG(output);
    free(output);
}