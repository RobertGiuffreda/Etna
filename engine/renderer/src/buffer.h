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