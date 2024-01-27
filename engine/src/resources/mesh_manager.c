#include "mesh_manager.h"

#include "containers/dynarray.h"

#include "core/etmemory.h"
#include "core/logger.h"
#include "core/etstring.h"

// TEMP: This should be made renderer implementation agnostic
#include "renderer/src/vk_types.h"
#include "renderer/src/renderer.h"
#include "renderer/src/buffer.h"

#include "renderer/src/utilities/vkinit.h"
#include "renderer/src/utilities/vkutils.h"
// TEMP: END

#define MAX_MESH_COUNT 512

struct mesh_manager {
    renderer_state* state;
    VkCommandPool upload_pool;
    VkFence* upload_fences;
    buffer* staging_buffers;
    u32 upload_count;

    // TEMP: Testing
    VkCommandPool test_pool;
    VkCommandBuffer test_buffer;
    VkFence test_fence;
    // TEMP: END

    mesh meshes[MAX_MESH_COUNT];
    u32 mesh_count;
};

b8 mesh_manager_initialize(mesh_manager** manager, struct renderer_state* state) {
    mesh_manager* new_manager = etallocate(sizeof(mesh_manager), MEMORY_TAG_RESOURCE);
    etzero_memory(new_manager, sizeof(mesh_manager));
    new_manager->state = state;

    VkCommandPoolCreateInfo upload_pool_info = init_command_pool_create_info(
        /* Flags: */ 0,
        state->device.graphics_qfi);
    VK_CHECK(vkCreateCommandPool(
        state->device.handle,
        &upload_pool_info,
        state->allocator,
        &new_manager->upload_pool));

    // TEMP: Testing uploading buffers
    VkCommandPoolCreateInfo test_pool_info = init_command_pool_create_info(
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        state->device.graphics_qfi);
    VK_CHECK(vkCreateCommandPool(
        state->device.handle,
        &test_pool_info,
        state->allocator,
        &new_manager->test_pool));

    VkCommandBufferAllocateInfo test_buff_alloc_info = init_command_buffer_allocate_info(
        new_manager->test_pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);
    VK_CHECK(vkAllocateCommandBuffers(
        state->device.handle,
        &test_buff_alloc_info,
        &new_manager->test_buffer));

    VkFenceCreateInfo test_fence_info = init_fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    VK_CHECK(vkCreateFence(
        state->device.handle,
        &test_fence_info,
        state->allocator,
        &new_manager->test_fence));
    // TEMP: END

    new_manager->upload_fences = dynarray_create(1, sizeof(VkFence));
    new_manager->staging_buffers = dynarray_create(1, sizeof(buffer));

    for (u32 i = 9; i < MAX_MESH_COUNT; ++i) {
        new_manager->meshes[i].id = INVALID_ID;
    }

    *manager = new_manager;
    return true;
}

void mesh_manager_shutdown(mesh_manager* manager) {
    dynarray_destroy(manager->upload_fences);
    dynarray_destroy(manager->staging_buffers);
    vkDestroyCommandPool(
        manager->state->device.handle,
        manager->upload_pool,
        manager->state->allocator);
    
    // TEMP: Testing sync
    vkDestroyCommandPool(
        manager->state->device.handle,
        manager->test_pool,
        manager->state->allocator);
    vkDestroyFence(
        manager->state->device.handle,
        manager->test_fence,
        manager->state->allocator);
    // TEMP: END

    for (u32 i = 0; i < manager->mesh_count; ++i) {
        dynarray_destroy(manager->meshes[i].surfaces);
        str_duplicate_free(manager->meshes[i].name);

        mesh_buffers* buffers = &manager->meshes[i].buffers;
        buffer_destroy(manager->state, &buffers->vertex_buffer);
        buffer_destroy(manager->state, &buffers->index_buffer);
    }
    etfree(manager, sizeof(mesh_manager), MEMORY_TAG_RESOURCE);
}

mesh* mesh_manager_get(mesh_manager* manager, u32 id) {
    return &manager->meshes[id];
}

b8 mesh_manager_submit_immediate_ref(mesh_manager* manager, mesh_config* config, mesh** out_mesh_ref) {
    ETASSERT(out_mesh_ref);
    if (manager->mesh_count >= MAX_MESH_COUNT)
        return false;

    mesh* new_mesh = &manager->meshes[manager->mesh_count];
    new_mesh->id = manager->mesh_count;
    new_mesh->name = str_duplicate_allocate(config->name);

    new_mesh->surfaces = dynarray_create_data(
        config->surface_count,
        sizeof(surface),
        config->surface_count,
        config->surfaces);

    new_mesh->buffers = upload_mesh_immediate(
        manager->state,
        config->index_count,
        config->indices,
        config->vertex_count,
        config->vertices
    );
    manager->mesh_count++;

    *out_mesh_ref = new_mesh;
    return true;
}

b8 mesh_manager_submit_immediate(mesh_manager* manager, mesh_config* config) {
    if (manager->mesh_count >= MAX_MESH_COUNT)
        return false;

    mesh* new_mesh = &manager->meshes[manager->mesh_count];
    new_mesh->id = manager->mesh_count;
    new_mesh->name = str_duplicate_allocate(config->name);

    new_mesh->surfaces = dynarray_create_data(
        config->surface_count,
        sizeof(surface),
        config->surface_count,
        config->surfaces);

    new_mesh->buffers = upload_mesh_immediate(
        manager->state,
        config->index_count,
        config->indices,
        config->vertex_count,
        config->vertices
    );
    manager->mesh_count++;
    return true;
}

// TODO: Pass in synchronization object maybe instead of calling wait in manager 
b8 mesh_manager_submit(mesh_manager* manager, mesh_config* config) {
    if (manager->mesh_count >= MAX_MESH_COUNT)
        return false;

    mesh* new_mesh = &manager->meshes[manager->mesh_count];
    new_mesh->id = manager->mesh_count;
    new_mesh->name = str_duplicate_allocate(config->name);

    new_mesh->surfaces = dynarray_create_data(
        config->surface_count,
        sizeof(surface),
        config->surface_count,
        config->surfaces);

    // Create staging buffer in manager's staging buffer queue
    u64 vertex_buffer_size = config->vertex_count * sizeof(vertex);
    u64 index_buffer_size = config->index_count * sizeof(u32);
    u64 staging_size = vertex_buffer_size + index_buffer_size;

    // Vertex buffer
    buffer_create(
        manager->state,
        vertex_buffer_size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
        | VK_BUFFER_USAGE_TRANSFER_DST_BIT
        | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &new_mesh->buffers.vertex_buffer);
    VkBufferDeviceAddressInfo vba_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = new_mesh->buffers.vertex_buffer.handle};
    new_mesh->buffers.vertex_buffer_address = vkGetBufferDeviceAddress(manager->state->device.handle, &vba_info);

    // Index buffer
    buffer_create(
        manager->state,
        index_buffer_size,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT
        | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &new_mesh->buffers.index_buffer);

    // Get index into upload staging buffers and fences dynamic arrays
    u32 upload_index = manager->upload_count++;
    dynarray_resize((void**)&manager->staging_buffers, manager->upload_count);
    dynarray_resize((void**)&manager->upload_fences, manager->upload_count);

    buffer* staging = &manager->staging_buffers[upload_index];
    
    buffer_create(manager->state, staging_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        staging);

    void* staging_memory;
    vkMapMemory(
        manager->state->device.handle,
        staging->memory,
        /* Offset: */ 0,
        staging_size,
        /*Flags*/ 0,
        &staging_memory);
    etcopy_memory(staging_memory, config->vertices, vertex_buffer_size);
    etcopy_memory((u8*)staging_memory + vertex_buffer_size, config->indices, index_buffer_size);
    vkUnmapMemory(manager->state->device.handle, staging->memory);

    IMM_SUBMIT_FENCE(manager->state, manager->upload_fences[upload_index], manager->upload_pool, cmd, {
        VkBufferCopy vertex_copy = {
            .dstOffset = 0,
            .srcOffset = 0,
            .size = vertex_buffer_size};
        VkBufferCopy index_copy = {
            .dstOffset = 0,
            .srcOffset = vertex_buffer_size,
            .size = index_buffer_size};
        vkCmdCopyBuffer(cmd, staging->handle, new_mesh->buffers.vertex_buffer.handle, 1, &vertex_copy);
        vkCmdCopyBuffer(cmd, staging->handle, new_mesh->buffers.index_buffer.handle, 1, &index_copy);
    });

    manager->mesh_count++;
    return true;
}

b8 mesh_manager_uploads_wait(mesh_manager* manager) {
    renderer_state* state = manager->state;
    VK_CHECK(vkWaitForFences(
        state->device.handle,
        manager->upload_count,
        manager->upload_fences,
        VK_TRUE,
        1000000000));
    for (u32 i = 0; i < manager->upload_count; ++i) {
        vkDestroyFence(
            state->device.handle,
            manager->upload_fences[i],
            state->allocator);
        buffer_destroy(state, &manager->staging_buffers[i]);
    }

    dynarray_clear(manager->upload_fences);
    dynarray_clear(manager->staging_buffers);
    manager->upload_count = 0;

    vkResetCommandPool(state->device.handle, manager->upload_pool, /* Flags: */ 0);
    return true;
}