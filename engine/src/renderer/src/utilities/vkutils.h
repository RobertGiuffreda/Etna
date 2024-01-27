#pragma once

#include "renderer/src/vk_types.h"

i32 find_memory_index(const VkPhysicalDeviceMemoryProperties* memory_properties,
    u32 memory_type_bits_requirement, VkMemoryPropertyFlags required_properties);

mesh_buffers upload_mesh_immediate(
    renderer_state* state,
    u32 index_count, u32* indices, 
    u32 vertex_count, vertex* vertices);

// TODO: Rename to represent that it allocates & begins command buffer recording
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
