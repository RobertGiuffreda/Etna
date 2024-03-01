#pragma once

#include "renderer/src/vk_types.h"

void buffer_create(
    renderer_state* state,
    u64 size,
    VkBufferUsageFlags usage_flags,
    VkMemoryPropertyFlags memory_property_flags,
    buffer* out_buffer);

void buffer_create_data(
    renderer_state* state,
    void* data,
    u64 size,
    VkBufferUsageFlags usage_flags,
    VkMemoryPropertyFlags memory_property_flags,
    buffer* out_buffer);

VkDeviceAddress buffer_get_address(renderer_state* state, buffer* buffer);

void buffer_destroy(renderer_state* state, buffer* buffer);

void buffer_barrier(
    VkCommandBuffer cmd,
    VkBuffer buffer,
    u64 offset,
    u64 size,
    VkAccessFlags2 src_access,
    VkAccessFlags2 dst_access,
    VkPipelineStageFlags2 src_stages,
    VkPipelineStageFlags2 dst_stages);