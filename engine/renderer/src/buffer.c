#include "buffer.h"

#include "core/logger.h"
#include "memory/etmemory.h"

#include "renderer/src/renderer.h"
#include "renderer/src/utilities/vkinit.h"
#include "renderer/src/utilities/vkutils.h"

void buffer_create(
    renderer_state* state,
    u64 size,
    VkBufferUsageFlags usage_flags,
    VkMemoryPropertyFlags memory_property_flags,
    buffer* out_buffer
) {
    // Does VkBufferCreateInfo need an initializer or does this 
    // needlessly obfuscate buffer creation
    VkBufferCreateInfo buffer_info = init_buffer_create_info(usage_flags, size);
    VK_CHECK(vkCreateBuffer(state->device.handle, &buffer_info, state->allocator, &out_buffer->handle));

    VkMemoryRequirements2 memory_requirements2 = init_memory_requirements2();
    VkBufferMemoryRequirementsInfo2 memory_requirements_info =
        init_buffer_memory_requirements_info2(out_buffer->handle);

    vkGetBufferMemoryRequirements2(state->device.handle, &memory_requirements_info, &memory_requirements2);
    VkMemoryRequirements memory_requirements = memory_requirements2.memoryRequirements;

    i32 memory_index = find_memory_index(
        &state->device.gpu_memory_props,
        memory_requirements.memoryTypeBits,
        memory_property_flags);
    if (memory_index == -1) {
        ETERROR("Supported memory types for buffer not found in physical memory properties.");
    }

    VkMemoryAllocateFlagsInfo alloc_flags_info;
    VkMemoryAllocateInfo alloc_info = init_memory_allocate_info(memory_requirements.size, memory_index);
    if (usage_flags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
        alloc_flags_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
        alloc_flags_info.pNext = 0;
        alloc_flags_info.deviceMask = 0;
        alloc_flags_info.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
        alloc_info.pNext = &alloc_flags_info;
    }
    VK_CHECK(vkAllocateMemory(
        state->device.handle,
        &alloc_info,
        state->allocator,
        &out_buffer->memory));
    VkBindBufferMemoryInfo bind_info = init_bind_buffer_memory_info(out_buffer->handle, out_buffer->memory, 0);
    VK_CHECK(vkBindBufferMemory2(state->device.handle, 1, &bind_info));
    out_buffer->size = memory_requirements.size;
}

void buffer_create_data(
    renderer_state* state,
    void* data,
    u64 size,
    VkBufferUsageFlags usage_flags,
    VkMemoryPropertyFlags memory_property_flags,
    buffer* out_buffer
) {
    buffer staging;
    buffer_create(
        state,
        size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        &staging
    );
    void* mapped_memory = 0;
    VK_CHECK(vkMapMemory(
        state->device.handle,
        staging.memory,
        /* Offset: */ 0,
        size,
        /* Flags: */ 0,
        &mapped_memory));
    etcopy_memory(mapped_memory, data, size);
    vkUnmapMemory(state->device.handle, staging.memory);

    // Create destination buffer
    buffer_create(
        state,
        size,
        usage_flags | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        memory_property_flags,
        out_buffer
    );

    IMMEDIATE_SUBMIT(state, cmd,
        VkBufferCopy buffer_copy = {
            .dstOffset = 0,
            .srcOffset = 0,
            .size = size};
        vkCmdCopyBuffer(cmd, staging.handle, out_buffer->handle, 1, &buffer_copy);
    );

    buffer_destroy(state, &staging);
}

VkDeviceAddress buffer_get_address(renderer_state* state, buffer* buffer) {
    VkBufferDeviceAddressInfo device_address_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .pNext = 0,
        .buffer = buffer->handle};
    return vkGetBufferDeviceAddress(state->device.handle, &device_address_info);
}

void buffer_destroy(renderer_state* state, buffer* buffer) {
    vkFreeMemory(state->device.handle, buffer->memory, state->allocator);
    buffer->memory = 0;
    vkDestroyBuffer(state->device.handle, buffer->handle, state->allocator);
    buffer->handle = 0;
    buffer->size = 0;
}

void buffer_barrier(
    VkCommandBuffer cmd,
    VkBuffer buffer,
    u64 offset,
    u64 size,
    VkAccessFlags2 src_access,
    VkAccessFlags2 dst_access,
    VkPipelineStageFlags2 src_stages,
    VkPipelineStageFlags2 dst_stages
) {
    VkBufferMemoryBarrier2 barrier = {
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
    VkDependencyInfo depends = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = 0,
        .dependencyFlags = 0,
        .memoryBarrierCount = 0,
        .pMemoryBarriers = NULL,
        .bufferMemoryBarrierCount = 1,
        .pBufferMemoryBarriers = &barrier,
        .imageMemoryBarrierCount = 0,
        .pImageMemoryBarriers = NULL,
    };
    vkCmdPipelineBarrier2(cmd, &depends);
}