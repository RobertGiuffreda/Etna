#pragma once

#include "renderer/src/vk_types.h"

void image2D_create(
    renderer_state* state,
    VkExtent3D extent,
    VkFormat format,
    VkImageUsageFlags usage_flags,
    VkImageAspectFlags aspect_flags,
    VkMemoryPropertyFlags memory_flags,
    image* out_image);

void image2D_create_data(
    renderer_state* state,
    void* data,
    VkExtent3D extent,
    VkFormat format,
    VkImageUsageFlags usage_flags,
    VkImageAspectFlags aspect_flags,
    VkMemoryPropertyFlags memory_flags,
    image* out_image);

void image_destroy(renderer_state* state, image* image);

void blit_image2D_to_image2D(
    VkCommandBuffer cmd,
    VkImage src,
    VkImage dst,
    VkExtent3D src_size,
    VkExtent3D dst_size,
    VkImageAspectFlags aspect_flags);

void image_barrier(
    VkCommandBuffer cmd,
    VkImage image,
    VkImageAspectFlags aspects,
    VkImageLayout old_layout,
    VkImageLayout new_layout,
    VkAccessFlags2 src_access_mask,
    VkAccessFlags2 dst_access_mask,
    VkPipelineStageFlags2 src_stage_mask,
    VkPipelineStageFlags2 dst_stage_mask);