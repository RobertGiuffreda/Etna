#include "image.h"

#include "renderer/src/utilities/vkinit.h"

#include "core/logger.h"

void image2D_create(
    renderer_state* state,
    VkExtent3D extent,
    VkFormat format,
    VkImageUsageFlagBits usage_flags,
    VkImageAspectFlags aspect_flags,
    VkMemoryPropertyFlags memory_flags,
    image* out_image)
{
    VkImageCreateInfo image_info = init_image2D_create_info(format, usage_flags, extent);
    VK_CHECK(vkCreateImage(state->device.handle, &image_info, state->allocator, &out_image->handle));

    VkMemoryRequirements2 memory_requirements2 = init_memory_requirements2();
    VkImageMemoryRequirementsInfo2 memory_requirements_info2 = 
        init_image_memory_requirements_info2(out_image->handle);
    
    vkGetImageMemoryRequirements2(state->device.handle, &memory_requirements_info2, &memory_requirements2);
    VkMemoryRequirements memory_requirements = memory_requirements2.memoryRequirements;

    u32 memory_index = find_memory_index(
        &state->device.gpu_memory_props,
        memory_requirements.memoryTypeBits,
        memory_flags);
    if (memory_index == -1) {
        ETERROR("Memory type with required memory type bits not found in physical memory properties.");
    }

    VkMemoryAllocateInfo alloc_info = init_memory_allocate_info(memory_requirements.size, memory_index);
    VK_CHECK(vkAllocateMemory(state->device.handle, &alloc_info, state->allocator, &out_image->memory));

    VkBindImageMemoryInfo bind_info = init_bind_image_memory_info(out_image->handle, out_image->memory, 0);
    VK_CHECK(vkBindImageMemory2(state->device.handle, 1, &bind_info));

    VkImageViewCreateInfo view_info = init_image_view2D_create_info(
        format, out_image->handle, aspect_flags);
    VK_CHECK(vkCreateImageView(state->device.handle, &view_info, state->allocator, &out_image->view));

    out_image->extent = extent;
    out_image->format = format;
}

void image2D_destroy(renderer_state* state, image* image) {
    image->format = VK_FORMAT_UNDEFINED;
    image->extent.width = 0;
    image->extent.height = 0;
    image->extent.depth = 0;

    vkDestroyImageView(state->device.handle, image->view, state->allocator);
    image->view = 0;
    vkFreeMemory(state->device.handle, image->memory, state->allocator);
    image->memory = 0;
    vkDestroyImage(state->device.handle, image->handle, state->allocator);
    image->handle = 0;
}

void blit_image2D_to_image2D(VkCommandBuffer cmd, VkImage src, VkImage dst, VkExtent3D image_size, VkImageAspectFlags aspect_flags) {
    VkImageBlit2 blit = {0};
    blit.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
    blit.pNext = 0;

    blit.srcOffsets[1].x = image_size.width;
    blit.srcOffsets[1].y = image_size.height;
    blit.srcOffsets[1].z = 1;

    blit.dstOffsets[1].x = image_size.width;
    blit.dstOffsets[1].y = image_size.height;
    blit.dstOffsets[1].z = 1;

    VkImageSubresourceLayers subresource = {0};
    subresource.aspectMask = aspect_flags;
    subresource.mipLevel = 0;
    subresource.layerCount = 1;
    subresource.baseArrayLayer = 0;

    blit.srcSubresource = subresource;
    blit.dstSubresource = subresource;
    
    VkBlitImageInfo2 blit_info = {0};
    blit_info.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
    blit_info.pNext = 0;
    blit_info.dstImage = dst;
    blit_info.srcImage = src;
    blit_info.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    blit_info.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    blit_info.filter = VK_FILTER_NEAREST;
    blit_info.regionCount = 1;
    blit_info.pRegions = &blit;

    vkCmdBlitImage2(cmd, &blit_info);
}

// TODO: Aspect flags passed as parameter || When Changing VkImage to (struct image). Using stored value
void image_barrier(
    VkCommandBuffer cmd,
    VkImage image,
    VkImageLayout old_layout,
    VkImageLayout new_layout,
    VkAccessFlags2 src_access_mask,
    VkAccessFlags2 dst_access_mask,
    VkPipelineStageFlags2 src_stage_mask,
    VkPipelineStageFlags2 dst_stage_mask)
{
    VkImageSubresourceRange subresource_range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1};
    VkImageMemoryBarrier2 barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext = 0,
        .srcStageMask = src_stage_mask,
        .dstStageMask = dst_stage_mask,
        .srcAccessMask = src_access_mask,
        .dstAccessMask = dst_access_mask,
        .oldLayout = old_layout,
        .newLayout = new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = subresource_range};

    VkDependencyInfo dependency = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = 0,
        .dependencyFlags = 0,
        .memoryBarrierCount = 0,
        .pBufferMemoryBarriers = 0,
        .bufferMemoryBarrierCount = 0,
        .pBufferMemoryBarriers = 0,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier};

    vkCmdPipelineBarrier2(cmd, &dependency);
}