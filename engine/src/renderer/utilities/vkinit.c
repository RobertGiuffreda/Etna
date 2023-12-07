#include "vkinit.h"

VkCommandPoolCreateInfo init_command_pool_create_info(
    VkCommandPoolCreateFlags flags,
    u32 queue_family_index)
{
    VkCommandPoolCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = 0,
        .flags = flags,
        .queueFamilyIndex = queue_family_index};
    return info;
}

VkCommandBufferAllocateInfo init_command_buffer_allocate_info(
    VkCommandPool command_pool,
    VkCommandBufferLevel level,
    u32 buffer_count)
{
    VkCommandBufferAllocateInfo info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = 0,
        .commandPool = command_pool,
        .level = level,
        .commandBufferCount = buffer_count};
    return info;
}

VkCommandBufferBeginInfo init_command_buffer_begin_info(
    VkCommandBufferUsageFlags flags)
{
    VkCommandBufferBeginInfo info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = 0,
        .flags = flags,
        .pInheritanceInfo = 0};
    return info;
}

VkCommandBufferSubmitInfo init_command_buffer_submit_info(
    VkCommandBuffer command_buffer)
{
    VkCommandBufferSubmitInfo info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .pNext = 0,
        .commandBuffer = command_buffer};
    return info;
}

VkFenceCreateInfo init_fence_create_info(VkFenceCreateFlags flags) {
    VkFenceCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = 0,
        .flags = flags};
    return info;
}

VkSemaphoreCreateInfo init_semaphore_create_info(VkSemaphoreCreateFlags flags) {
    VkSemaphoreCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = 0,
        .flags = flags};
    return info;
}

VkSemaphoreSubmitInfo init_semaphore_submit_info(
    VkSemaphore semaphore,
    VkPipelineStageFlags2 stage_mask)
{
    VkSemaphoreSubmitInfo info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .pNext = 0,
        .semaphore = semaphore,
        .stageMask = stage_mask};
    return info;
}

VkSubmitInfo2 init_submit_info2(
    u32 wait_semaphore_info_count, const VkSemaphoreSubmitInfo* pwait_semaphore_infos,
    u32 command_buffer_info_count, const VkCommandBufferSubmitInfo* pcommand_buffer_infos,
    u32 signal_semaphore_info_count, const VkSemaphoreSubmitInfo* psignal_semaphore_infos)
{
    VkSubmitInfo2 info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .pNext = 0,
        .waitSemaphoreInfoCount = wait_semaphore_info_count,
        .pWaitSemaphoreInfos = pwait_semaphore_infos,
        .commandBufferInfoCount = command_buffer_info_count,
        .pCommandBufferInfos = pcommand_buffer_infos,
        .signalSemaphoreInfoCount = signal_semaphore_info_count,
        .pSignalSemaphoreInfos = psignal_semaphore_infos};
    return info;
}

VkImageSubresourceRange init_image_subresource_range(
    VkImageAspectFlags aspect_flags)
{
    VkImageSubresourceRange sub_range = {
        .aspectMask = aspect_flags,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1};
    return sub_range;
}

VkImageCreateInfo init_image2D_create_info(
    VkFormat format,
    VkImageUsageFlags usage_flags,
    VkExtent3D extent)
{
    VkImageCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = 0,
        .flags = 0,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = extent,
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = usage_flags};
    return info;
}

VkImageViewCreateInfo init_image_view2D_create_info(
    VkFormat format,
    VkImage image,
    VkImageAspectFlags aspect_flags)
{
    VkImageViewCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = 0,
        .flags = 0,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .subresourceRange = {
            .aspectMask = aspect_flags,
            .levelCount = 1,
            .baseMipLevel = 0,
            .layerCount = 1,
            .baseArrayLayer = 0}};
    return info;
}

VkMemoryRequirements2 init_memory_requirements2(void) {
    VkMemoryRequirements2 requirements = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
        .pNext = 0,
        .memoryRequirements = {0}};
    return requirements;
}

VkImageMemoryRequirementsInfo2 init_image_memory_requirements_info2(VkImage image) {
    VkImageMemoryRequirementsInfo2 info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2,
        .pNext = 0,
        .image = image};
    return info;
}

VkMemoryAllocateInfo init_memory_allocate_info(VkDeviceSize allocation_size, u32 memory_type_index) {
    VkMemoryAllocateInfo info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = 0,
        .allocationSize = allocation_size,
        .memoryTypeIndex = memory_type_index};
    return info;
}

VkBindImageMemoryInfo init_bind_image_memory_info(
    VkImage image, VkDeviceMemory memory, VkDeviceSize memory_offset)
{
    VkBindImageMemoryInfo info = {
        .sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO,
        .pNext = 0,
        .image = image,
        .memory = memory,
        .memoryOffset = memory_offset};
    return info;
}

VkDeviceImageMemoryRequirements init_device_image_memory_requirements(
    const VkImageCreateInfo* image_create_info)
{
    VkDeviceImageMemoryRequirements requirements = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_IMAGE_MEMORY_REQUIREMENTS,
        .pNext = 0,
        .pCreateInfo = image_create_info,
        .planeAspect = 0};
    return requirements;
}

VkPhysicalDeviceMemoryProperties2 init_physical_device_memory_properties2(void) {
    VkPhysicalDeviceMemoryProperties2 properties = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2,
        .pNext = 0};
    return properties;
}

VkShaderModuleCreateInfo init_shader_module_create_info(void) {
    VkShaderModuleCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = 0};
    return info;
}

VkPipelineLayoutCreateInfo init_pipline_layout_create_info(void) {
    VkPipelineLayoutCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = 0};
    return info;
}

VkDescriptorSetLayoutCreateInfo init_descriptor_set_layout_create_info(void) {
    VkDescriptorSetLayoutCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = 0};
    return info;
}

VkDescriptorPoolCreateInfo init_descriptor_pool_create_info(void) {
    VkDescriptorPoolCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = 0};
    return info;
}

VkDescriptorSetAllocateInfo init_descriptor_set_allocate_info(void) {
    VkDescriptorSetAllocateInfo info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = 0};
    return info;
}

VkPipelineLayoutCreateInfo init_pipeline_layout_create_info(void) {
    VkPipelineLayoutCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = 0};
    return info;
}

VkPipelineShaderStageCreateInfo init_pipeline_shader_stage_create_info(void) {
    VkPipelineShaderStageCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = 0};
    return info;
}

VkComputePipelineCreateInfo init_compute_pipeline_create_info(void) {
    VkComputePipelineCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = 0};
    return info;
}

// TODO: Move this to a utilities file
i32 find_memory_index(const VkPhysicalDeviceMemoryProperties* memory_properties,
    u32 memory_type_bits_requirement, VkMemoryPropertyFlags required_properties)
{
    for (u32 i = 0; i < memory_properties->memoryTypeCount; ++i) {
        b8 is_required_memory_type = (1 << i) & memory_type_bits_requirement;
        b8 has_required_properties = (memory_properties->memoryTypes[i].propertyFlags & required_properties) == required_properties;
        if (is_required_memory_type && has_required_properties) {
            return i;
        }
    }
    
    return -1;
}