#pragma once
#include "renderer/src/vk_types.h"
// TODO: Macros

i32 find_memory_index(const VkPhysicalDeviceMemoryProperties* memory_properties,
    u32 memory_type_bits_requirement, VkMemoryPropertyFlags required_properties);

#define CODE_BLOCK(...) \
do {                    \
    __VA_ARGS__;        \
} while(0);             \

void _cmd_allocate_begin(renderer_state* state, VkCommandPool pool, VkCommandBuffer* cmd);
void _cmd_end_submit(renderer_state* state, VkCommandBuffer cmd, VkFence fence);

void _fence_create(renderer_state* state, VkFence* fence);

#define IMM_SUBMIT_FENCE(state, fence, pool, cmd_n, ...)    \
do {                                                        \
    VkCommandBuffer cmd_n;                                  \
    _cmd_allocate_begin(state, pool, &cmd_n);               \
    do {                                                    \
        __VA_ARGS__;                                        \
    } while(0);                                             \
    _fence_create(state, &fence);                           \
    _cmd_end_submit(state, cmd_n, fence);                   \
} while(0);                                                 \

void _immediate_begin(renderer_state* state);
void _immediate_end(renderer_state* state);

#define IMMEDIATE_SUBMIT(state, cmd_n, ...)     \
do {                                            \
    _immediate_begin(state);                    \
    VkCommandBuffer cmd_n = state->imm_buffer;  \
    do {                                        \
        __VA_ARGS__;                            \
    } while (0);                                \
    _immediate_end(state);                      \
} while(0);                                     \

vertex_index_buffers vertex_index_buffers_create(
    renderer_state* state,
    u32 index_count,
    u32* indices, 
    u32 vertex_count,
    vertex* vertices,
    u32 extra_vertex_count
);