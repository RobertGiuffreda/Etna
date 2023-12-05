#pragma once

#include "renderer_types.h"

VkCommandPoolCreateInfo init_command_pool_create_info(
    VkCommandPoolCreateFlags flags,
    u32 queue_family_index);

VkCommandBufferAllocateInfo init_command_buffer_allocate_info(
    VkCommandPool command_pool,
    VkCommandBufferLevel level,
    u32 buffer_count);

VkCommandBufferBeginInfo init_command_buffer_begin_info(
    VkCommandBufferUsageFlags flags);

VkCommandBufferSubmitInfo init_command_buffer_submit_info(
    VkCommandBuffer command_buffer);

VkFenceCreateInfo init_fence_create_info(VkFenceCreateFlags flags);

VkSemaphoreCreateInfo init_semaphore_create_info(VkSemaphoreCreateFlags flags);

VkSemaphoreSubmitInfo init_semaphore_submit_info(
    VkSemaphore semaphore,
    VkPipelineStageFlags2 stage_mask);

VkSubmitInfo2 init_submit_info2(
    u32 wait_semaphore_info_count, const VkSemaphoreSubmitInfo* pwait_semaphore_infos,
    u32 command_buffer_info_count, const VkCommandBufferSubmitInfo* pcommand_buffer_infos,
    u32 signal_semaphore_info_count, const VkSemaphoreSubmitInfo* psignal_semaphore_infos);

VkImageSubresourceRange init_image_subresource_range(
    VkImageAspectFlags aspect_flags);

VkImageCreateInfo init_image2D_create_info(
    VkFormat format,
    VkImageUsageFlags usage_flags,
    VkExtent3D extent);

VkImageViewCreateInfo init_image_view2D_create_info(
    VkFormat format,
    VkImage image,
    VkImageAspectFlags aspect_flags);

/* Initializers TODO:
VkMemoryAllocateInfo

VkBufferMemoryBarrier2
VkImageMemoryBarrier2
VkDependencyInfo

VkSwapchainCreateInfoKHR
*/

VkMemoryRequirements2 init_memory_requirements2(void);

VkImageMemoryRequirementsInfo2 init_image_memory_requirements_info2(VkImage image);

VkMemoryAllocateInfo init_memory_allocate_info(VkDeviceSize allocationSize,
    u32 memoryTypeIndex);

VkBindImageMemoryInfo init_bind_image_memory_info(
    VkImage image, VkDeviceMemory memory, VkDeviceSize memory_offset);

VkDeviceImageMemoryRequirements init_device_image_memory_requirements(
    const VkImageCreateInfo* image_create_info);

VkPhysicalDeviceMemoryProperties2 init_physical_device_memory_properties2(void);

i32 find_memory_index(const VkPhysicalDeviceMemoryProperties* memory_properties,
    u32 memory_type_bits_requirement, VkMemoryPropertyFlags required_properties);