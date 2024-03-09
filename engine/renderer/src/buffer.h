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

static inline VkBufferMemoryBarrier2 buffer_memory_barrier(
    VkBuffer buffer,
    u64 offset,
    u64 size,
    VkAccessFlags2 src_access,
    VkAccessFlags2 dst_access,
    VkPipelineStageFlags2 src_stages,
    VkPipelineStageFlags2 dst_stages
) {
    return (VkBufferMemoryBarrier2){
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
        .pNext = 0,
        .buffer = buffer,
        .offset = offset,
        .size = size,
        .srcAccessMask = src_access,
        .dstAccessMask = dst_access,
        .srcStageMask = src_stages,
        .dstStageMask = dst_stages,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    };
}