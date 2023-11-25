#include "etmemory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct memory_state {
    u64 total_allocated;
    u64 allocated[MEMORY_TAG_MAX];
};

static struct memory_state state;

void memory_init(void) {
    memset((void*)&state, 0, sizeof(struct memory_state));
}

void* etallocate(u64 size, memory_tag tag) {
    state.total_allocated += size; 
    state.allocated[tag] += size;
    return malloc(size);
}

void etfree(void* block, u64 size, memory_tag tag) {
    state.total_allocated -= size; 
    state.allocated[tag] -= size;
    free(block);
}

u64 get_total_allocs(void) {
    return state.total_allocated;
}