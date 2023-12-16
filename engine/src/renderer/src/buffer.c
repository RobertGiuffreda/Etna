#include "buffer.h"

#include "core/logger.h"

void buffer_create(
    renderer_state* state,
    u64 size,
    VkBufferUsageFlags usage_flags,
    VkMemoryPropertyFlags memory_property_flags,
    buffer* out_buffer)
{
    // Does buffer create need an initializer or does this 
    // needlessly obfuscate buffer creation
    VkBufferCreateInfo buffer_info = init_buffer_create_info(usage_flags, size);
    VK_CHECK(vkCreateBuffer(state->device.handle, &buffer_info, state->allocator, &out_buffer->handle));

    VkMemoryRequirements2 memory_requirements2 = init_memory_requirements2();
    VkBufferMemoryRequirementsInfo2 memory_requirements_info =
        init_buffer_memory_requirements_info2(out_buffer->handle);

    vkGetBufferMemoryRequirements2(state->device.handle, &memory_requirements_info, &memory_requirements2);
    VkMemoryRequirements memory_requirements = memory_requirements2.memoryRequirements;

    u32 memory_index = find_memory_index(
        &state->device.gpu_memory_props,
        memory_requirements.memoryTypeBits,
        memory_property_flags);
    if (memory_index == -1) {
        ETERROR("Memory type with required memory type bits for buffer not found in physical memory properties");
    }

    VkMemoryAllocateInfo alloc_info = init_memory_allocate_info(size, memory_index); 
    VK_CHECK(vkAllocateMemory(
        state->device.handle,
        &alloc_info,
        state->allocator,
        &out_buffer->memory));
    
    VkBindBufferMemoryInfo bind_info = init_bind_buffer_memory_info(out_buffer->handle, out_buffer->memory, 0);
    VK_CHECK(vkBindBufferMemory2(state->device.handle, 1, &bind_info));
}

void buffer_destroy(renderer_state* state, buffer* buffer) {
    vkFreeMemory(state->device.handle, buffer->memory, state->allocator);
    buffer->memory = 0;
    vkDestroyBuffer(state->device.handle, buffer->handle, state->allocator);
    buffer->handle = 0;
}