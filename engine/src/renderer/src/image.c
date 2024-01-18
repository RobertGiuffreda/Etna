#include "image.h"

#include "core/etmemory.h"
#include "core/logger.h"

#include "renderer/src/utilities/vkinit.h"
#include "renderer/src/renderer.h"
#include "renderer/src/buffer.h"

// TODO: Multisampling bits in ImageCreateInfo
// TODO: Check if tiling and sample settings are supported for the specific image format
void image2D_create(
    renderer_state* state,
    VkExtent3D extent,
    VkFormat format,
    VkImageUsageFlags usage_flags,
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
        ETERROR("Memory type with required memory type bits for image not found in physical memory properties.");
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
    out_image->aspects = aspect_flags;
}

void image2D_create_data(
    renderer_state* state,
    void* data,
    VkExtent3D extent,
    VkFormat format,
    VkImageUsageFlags usage_flags,
    VkImageAspectFlags aspect_flags,
    VkMemoryPropertyFlags memory_flags,
    image* out_image)
{
    // Create staging buffer and upload data to it

    // TODO: Change 4 to a get element size from vkformat utility function
    u64 data_size = extent.width * extent.height * extent.depth * 4;
    buffer staging = {0};
    buffer_create(
        state,
        data_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &staging);
    void* mapped_memory;
    vkMapMemory(
        state->device.handle,
        staging.memory,
        /* offset: */ 0,
        data_size,
        /* flags: */ 0,
        &mapped_memory);
    etcopy_memory(mapped_memory, data, data_size);
    vkUnmapMemory(state->device.handle, staging.memory);

    // Create image to upload data to
    image2D_create(
        state,
        extent,
        format,
        usage_flags | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        aspect_flags,
        memory_flags,
        out_image);
    
    IMMEDIATE_SUBMIT(state, cmd, {
        // Transition image to optimal transfer destination layout
        image_barrier(cmd, out_image->handle, out_image->aspects,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_ACCESS_2_NONE, VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT
        );

        VkBufferImageCopy2 cpy = init_buffer_image_copy2();
        cpy.bufferOffset = 0;
        cpy.bufferRowLength = 0;
        cpy.bufferImageHeight = 0;

        cpy.imageSubresource.aspectMask = out_image->aspects;
        cpy.imageSubresource.mipLevel = 0;
        cpy.imageSubresource.baseArrayLayer = 0;
        cpy.imageSubresource.layerCount = 1;
        cpy.imageOffset.x = 0;
        cpy.imageOffset.y = 0;
        cpy.imageOffset.z = 0;
        cpy.imageExtent = extent;

        VkCopyBufferToImageInfo2 copy_info = init_copy_buffer_to_image_info2(
            staging.handle,
            out_image->handle,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        copy_info.regionCount = 1;
        copy_info.pRegions = &cpy;

        vkCmdCopyBufferToImage2(cmd, &copy_info);

        // Transition image to shader readonly optimal for now
        image_barrier(cmd, out_image->handle, out_image->aspects,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
        );
    });
    buffer_destroy(state, &staging);
}

void image_destroy(renderer_state* state, image* image) {
    image->format = VK_FORMAT_UNDEFINED;
    image->extent.width = 0;
    image->extent.height = 0;
    image->extent.depth = 0;
    image->aspects = VK_IMAGE_ASPECT_NONE;

    vkDestroyImageView(state->device.handle, image->view, state->allocator);
    image->view = 0;
    vkFreeMemory(state->device.handle, image->memory, state->allocator);
    image->memory = 0;
    vkDestroyImage(state->device.handle, image->handle, state->allocator);
    image->handle = 0;
}

// TODO: Have this function take struct image instead of parameters 
void blit_image2D_to_image2D(
    VkCommandBuffer cmd,
    VkImage src, VkImage dst,
    VkExtent3D src_size, VkExtent3D dst_size,
    VkImageAspectFlags aspect_flags)
{
    VkImageBlit2 blit = {0};
    blit.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
    blit.pNext = 0;

    blit.srcOffsets[1].x = src_size.width;
    blit.srcOffsets[1].y = src_size.height;
    blit.srcOffsets[1].z = src_size.depth;

    blit.dstOffsets[1].x = dst_size.width;
    blit.dstOffsets[1].y = dst_size.height;
    blit.dstOffsets[1].z = dst_size.depth;

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
    VkImageAspectFlags aspects,
    VkImageLayout old_layout,
    VkImageLayout new_layout,
    VkAccessFlags2 src_access_mask,
    VkAccessFlags2 dst_access_mask,
    VkPipelineStageFlags2 src_stage_mask,
    VkPipelineStageFlags2 dst_stage_mask)
{
    VkImageSubresourceRange subresource_range = {
        .aspectMask = aspects,
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