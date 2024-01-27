#include "vkutils.h"

#include "renderer/src/utilities/vkinit.h"

#include "renderer/src/renderer.h"
#include "renderer/src/buffer.h"

/** TODO:
 * Documentation explaining what this does and how it works
 * NOTE: Cache/store the indices of known memory usage bit indices and 
 * start traversing from that index for improvement
 */
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

// TODO: Remove the VK_MEMORY_PROPERTY_HOST_COHERENT_BIT bit from the staging buffer and flush manually
// TODO: Use queue from dedicated transfer queue family(state->device.transfer_queue) to do transfers
mesh_buffers upload_mesh_immediate(renderer_state* state, u32 index_count, u32* indices, u32 vertex_count, vertex* vertices) {
    const u64 vertex_buffer_size = vertex_count * sizeof(vertex);
    const u64 index_buffer_size = index_count * sizeof(u32);

    mesh_buffers new_surface;

    // Create Vertex Buffer
    buffer_create(
        state,
        vertex_buffer_size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
        | VK_BUFFER_USAGE_TRANSFER_DST_BIT
        | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &new_surface.vertex_buffer);
    
    VkBufferDeviceAddressInfo device_address_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .pNext = 0,
        .buffer = new_surface.vertex_buffer.handle};
    // Get Vertex buffer address
    new_surface.vertex_buffer_address = vkGetBufferDeviceAddress(state->device.handle, &device_address_info);

    // Create Index Buffer
    buffer_create(
        state,
        index_buffer_size,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT
        | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &new_surface.index_buffer);
    
    // Create staging buffer to transfer memory from VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT to VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    buffer staging;
    buffer_create(
        state,
        vertex_buffer_size + index_buffer_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &staging);

    void* mapped_memory;
    vkMapMemory(state->device.handle, staging.memory, 0, vertex_buffer_size + index_buffer_size, 0, &mapped_memory);

    etcopy_memory(mapped_memory, vertices, vertex_buffer_size);
    etcopy_memory((u8*)mapped_memory + vertex_buffer_size, indices, index_buffer_size);

    vkUnmapMemory(state->device.handle, staging.memory);

    IMMEDIATE_SUBMIT(state, cmd, {
        VkBufferCopy vertex_copy = {
            .dstOffset = 0,
            .srcOffset = 0,
            .size = vertex_buffer_size};

        vkCmdCopyBuffer(cmd, staging.handle, new_surface.vertex_buffer.handle, 1, &vertex_copy);

        VkBufferCopy index_copy = {
            .dstOffset = 0,
            .srcOffset = vertex_buffer_size,
            .size = index_buffer_size};

        vkCmdCopyBuffer(cmd, staging.handle, new_surface.index_buffer.handle, 1, &index_copy);
    });

    buffer_destroy(state, &staging);

    return new_surface;
}

void _cmd_allocate_begin(renderer_state* state, VkCommandPool pool, VkCommandBuffer* cmd) {
    VkCommandBufferAllocateInfo cmd_alloc = init_command_buffer_allocate_info(pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);
    VK_CHECK(vkAllocateCommandBuffers(state->device.handle, &cmd_alloc, cmd));

    VkCommandBufferBeginInfo cmd_begin = init_command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(*cmd, &cmd_begin));
}

void _fence_create(renderer_state* state, VkFence* fence) {
    VkFenceCreateInfo fence_info = init_fence_create_info(/* Flags: */ 0);
    VK_CHECK(vkCreateFence(state->device.handle, &fence_info, state->allocator, fence));
}

void _cmd_end_submit(renderer_state* state, VkCommandBuffer cmd, VkFence fence) {
    VK_CHECK(vkEndCommandBuffer(cmd));
    VkCommandBufferSubmitInfo cmd_submit = init_command_buffer_submit_info(cmd);
    VkSubmitInfo2 submit_info = init_submit_info2(0, NULL, 1, &cmd_submit, 0, NULL);

    VK_CHECK(vkQueueSubmit2(state->device.graphics_queue, 1, &submit_info, fence));
}