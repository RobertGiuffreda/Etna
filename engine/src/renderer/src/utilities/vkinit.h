#pragma once

#include "renderer/src/vk_types.h"

// TODO: Move find_memory_index to another file

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

VkBufferCreateInfo init_buffer_create_info(VkBufferUsageFlags usage_flags, u64 size);

/* Initializers TODO:
VkBufferMemoryBarrier2
VkImageMemoryBarrier2
VkDependencyInfo

VkSwapchainCreateInfoKHR
*/

VkMemoryRequirements2 init_memory_requirements2(void);

VkImageMemoryRequirementsInfo2 init_image_memory_requirements_info2(VkImage image);

VkBufferMemoryRequirementsInfo2 init_buffer_memory_requirements_info2(VkBuffer buffer);

VkMemoryAllocateInfo init_memory_allocate_info(VkDeviceSize allocationSize,
    u32 memoryTypeIndex);

VkMemoryAllocateFlagsInfo init_memory_allocate_flags_info(VkMemoryAllocateFlags flags);

VkBindImageMemoryInfo init_bind_image_memory_info(
    VkImage image, VkDeviceMemory memory, VkDeviceSize memory_offset);

VkBindBufferMemoryInfo init_bind_buffer_memory_info(
    VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize memory_offset);

VkDeviceImageMemoryRequirements init_device_image_memory_requirements(
    const VkImageCreateInfo* image_create_info);

VkPhysicalDeviceMemoryProperties2 init_physical_device_memory_properties2(void);

VkShaderModuleCreateInfo init_shader_module_create_info(void);

// TODO: Determine parameters to pass
VkPipelineLayoutCreateInfo init_pipline_layout_create_info(void);

VkDescriptorSetLayoutCreateInfo init_descriptor_set_layout_create_info(void);

VkDescriptorPoolCreateInfo init_descriptor_pool_create_info(void);

VkDescriptorSetAllocateInfo init_descriptor_set_allocate_info(void);

VkPipelineLayoutCreateInfo init_pipeline_layout_create_info(void);

VkPipelineShaderStageCreateInfo init_pipeline_shader_stage_create_info(void);

VkComputePipelineCreateInfo init_compute_pipeline_create_info(void);

VkRenderingAttachmentInfo init_color_attachment_info(VkImageView view, VkClearValue* clear, VkImageLayout layout);

VkRenderingAttachmentInfo init_depth_attachment_info(VkImageView view, VkImageLayout layout);

VkRenderingInfo init_rendering_info(VkExtent2D render_extent, VkRenderingAttachmentInfo* color_attachment, VkRenderingAttachmentInfo* depthAttachment);

VkBufferImageCopy2 init_buffer_image_copy2(void);

VkCopyBufferToImageInfo2 init_copy_buffer_to_image_info2(
    VkBuffer buffer,
    VkImage image,
    VkImageLayout layout);

i32 find_memory_index(const VkPhysicalDeviceMemoryProperties* memory_properties,
    u32 memory_type_bits_requirement, VkMemoryPropertyFlags required_properties);

gpu_mesh_buffers upload_mesh(
    renderer_state* state,
    u32 index_count, u32* indices, 
    u32 vertex_count, vertex* vertices);

// TODO: Add immediate submit and begin into this class as well as macros