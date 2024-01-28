#include "descriptor.h"

#include "containers/dynarray.h"

#include "core/logger.h"

#include "renderer/src/utilities/vkinit.h"
#include "renderer/src/renderer.h"

/** TODO: All three objects
 * Support for descriptor count greater than one
 * Less unwieldy function & object names 
*/

// NOTE: Descriptor binding count greater than 1 is unsupported at the moment.
// layout (set = 1, binding = 4) uniform usampler2D test_tex[2];
// layout(set = 1, binding = 3) uniform test_block {
// 	ivec4 test_ivec[4];
// 	uvec4 test_uvec[4];
// } test[2];

dsl_builder descriptor_set_layout_builder_create(void) {
    dsl_builder builder;
    builder.bindings = dynarray_create(0, sizeof(VkDescriptorSetLayoutBinding));
    return builder;
}

void descriptor_set_layout_builder_clear(dsl_builder* builder) {
    dynarray_clear(builder->bindings);
}

void descriptor_set_layout_builder_destroy(dsl_builder* builder) {
    dynarray_destroy(builder->bindings);
}

void descriptor_set_layout_builder_add_binding(
    dsl_builder* builder,
    u32 binding,
    u32 count,
    VkDescriptorType type,
    VkShaderStageFlags stage_flags)
{
    VkDescriptorSetLayoutBinding bind = {
        .binding = binding,
        .descriptorCount = count,
        .descriptorType = type,
        .stageFlags = stage_flags};
    dynarray_push((void**)&builder->bindings, &bind);
}

VkDescriptorSetLayout descriptor_set_layout_builder_build(dsl_builder* builder, renderer_state* state) {
    VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = 0,
        .bindingCount = dynarray_length(builder->bindings),
        .pBindings = builder->bindings,
        .flags = 0};
    VkDescriptorSetLayout layout;
    VK_CHECK(vkCreateDescriptorSetLayout(state->device.handle, &layout_info, state->allocator, &layout));
    return layout;
}

ds_writer descriptor_set_writer_create_initialize(void) {
    ds_writer writer;
    descriptor_set_writer_initialize(&writer);
    return writer;
}

void descriptor_set_writer_initialize(ds_writer* writer) {
    writer->image_infos = dynarray_create(0, sizeof(VkDescriptorImageInfo));
    writer->buffer_infos = dynarray_create(0, sizeof(VkDescriptorBufferInfo));
    writer->writes = dynarray_create(0, sizeof(VkWriteDescriptorSet));
}

void descriptor_set_writer_shutdown(ds_writer* writer) {
    dynarray_destroy(writer->image_infos);
    dynarray_destroy(writer->buffer_infos);
    dynarray_destroy(writer->writes);
}

// TODO: Add descriptor count to function parameters
void descriptor_set_writer_write_image(
    ds_writer* writer,
    u32 binding,
    VkImageView view,
    VkSampler sampler,
    VkImageLayout layout,
    VkDescriptorType type)
{
    // NOTE: The destination set value is not set in this function
    // NOTE: Descriptor count is hard coded to one in this function
    VkDescriptorImageInfo image_info = {
        .imageView = view,
        .sampler = sampler,
        .imageLayout = layout};
    dynarray_push((void**)&writer->image_infos, &image_info);

    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = 0,
        .dstBinding = binding,
        .dstSet = VK_NULL_HANDLE,
        .descriptorCount = 1,
        .descriptorType = type,
        .pImageInfo = &writer->image_infos[dynarray_length(writer->image_infos) - 1]};

    dynarray_push((void**)&writer->writes, &write);
}

void descriptor_set_writer_write_buffer(
    ds_writer* writer,
    u32 binding,
    VkBuffer buffer,
    u64 size,
    u64 offset,
    VkDescriptorType type)
{
    VkDescriptorBufferInfo buffer_info = {
        .buffer = buffer,
        .offset = offset,
        .range = size};
    dynarray_push((void**)&writer->buffer_infos, &buffer_info);

    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = 0,
        .dstBinding = binding,
        .dstSet = VK_NULL_HANDLE,
        .descriptorCount = 1,
        .descriptorType = type,
        .pBufferInfo = &writer->buffer_infos[dynarray_length(writer->buffer_infos) - 1]};
    dynarray_push((void**)&writer->writes, &write);
}

void descriptor_set_writer_clear(ds_writer* writer) {
    dynarray_clear(writer->image_infos);
    dynarray_clear(writer->buffer_infos);
    dynarray_clear(writer->writes);
}
void descriptor_set_writer_update_set(ds_writer* writer, VkDescriptorSet set, renderer_state* state) {
    for (u32 i = 0; i < dynarray_length(writer->writes); ++i) {
        writer->writes[i].dstSet = set;
    }

    vkUpdateDescriptorSets(state->device.handle, (u32)dynarray_length(writer->writes), writer->writes, 0, 0);
}

void descriptor_set_allocator_initialize(
    ds_allocator* allocator,
    u32 initial_sets,
    u32 pool_size_count,
    pool_size_ratio* pool_sizes,
    renderer_state* state)
{
    allocator->pool_size_count = pool_size_count;
    allocator->pool_sizes = etallocate(sizeof(pool_size_ratio) * pool_size_count, MEMORY_TAG_RENDERER);
    etcopy_memory(allocator->pool_sizes, pool_sizes, sizeof(pool_size_ratio) * pool_size_count);
    allocator->ready_pools = dynarray_create(0, sizeof(VkDescriptorPool));
    allocator->full_pools = dynarray_create(0, sizeof(VkDescriptorPool));
    allocator->sets_per_pool = initial_sets;

    // Create first pool
    VkDescriptorPool first_pool = 
        descriptor_set_allocator_create_pool(allocator, state);
    
    allocator->sets_per_pool *= 1.5f;
    dynarray_push((void**)&allocator->ready_pools, &first_pool);
}

void descriptor_set_allocator_shutdown(
    ds_allocator* allocator,
    renderer_state* state)
{
    descriptor_set_allocator_destroy_pools(allocator, state);
    etfree(allocator->pool_sizes, sizeof(pool_size_ratio) * allocator->pool_size_count, MEMORY_TAG_RENDERER);
    dynarray_destroy(allocator->ready_pools);
    dynarray_destroy(allocator->full_pools);
    allocator->pool_size_count = 0;
    allocator->sets_per_pool = 0;
}

void descriptor_set_allocator_clear_pools(
    ds_allocator* allocator,
    renderer_state* state)
{
    u32 rpool_len = dynarray_length(allocator->ready_pools);
    for (u32 i = 0; i < rpool_len; ++i) {
        VkDescriptorPool pool = allocator->ready_pools[i];
        vkResetDescriptorPool(state->device.handle, pool, 0);
    }

    u32 fpool_len = dynarray_length(allocator->full_pools);
    for (u32 i = 0; i < fpool_len; ++i) {
        VkDescriptorPool pool = allocator->full_pools[i];
        vkResetDescriptorPool(state->device.handle, pool, 0);
        dynarray_push((void**)&allocator->ready_pools, &pool);
    }
    dynarray_clear(allocator->full_pools);
}

void descriptor_set_allocator_destroy_pools(
    ds_allocator* allocator,
    renderer_state* state)
{
    u32 rpool_len = dynarray_length(allocator->ready_pools);
    for (u32 i = 0; i < rpool_len; ++i) {
        VkDescriptorPool pool = allocator->ready_pools[i];
        vkDestroyDescriptorPool(state->device.handle, pool, state->allocator);
    }
    dynarray_clear(allocator->ready_pools);

    u32 fpool_len = dynarray_length(allocator->full_pools);
    for (u32 i = 0; i < fpool_len; ++i) {
        VkDescriptorPool pool = allocator->full_pools[i];
        vkDestroyDescriptorPool(state->device.handle, pool, state->allocator);
    }
    dynarray_clear(allocator->full_pools);
}

VkDescriptorSet descriptor_set_allocator_allocate(
    ds_allocator* allocator,
    VkDescriptorSetLayout layout,
    renderer_state* state)
{
    VkDescriptorPool to_use = 
        descriptor_set_allocator_get_pool(allocator, state);
    VkDescriptorSetAllocateInfo alloc_info = init_descriptor_set_allocate_info();
    alloc_info.descriptorPool = to_use;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &layout;

    VkDescriptorSet ds;
    VkResult result = vkAllocateDescriptorSets(state->device.handle, &alloc_info, &ds);

    if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {
        dynarray_push((void**)&allocator->full_pools, &to_use);

        to_use = descriptor_set_allocator_get_pool(allocator, state);
        alloc_info.descriptorPool = to_use;

        VK_CHECK(vkAllocateDescriptorSets(state->device.handle, &alloc_info, &ds));
    }

    dynarray_push((void**)&allocator->ready_pools, &to_use);
    return ds;
}

VkDescriptorPool descriptor_set_allocator_get_pool(
    ds_allocator* allocator,
    renderer_state* state)
{
    VkDescriptorPool new_pool;
    if (dynarray_length(allocator->ready_pools) != 0) {
        dynarray_pop(allocator->ready_pools, &new_pool);
    } else {
        new_pool = descriptor_set_allocator_create_pool(allocator, state);
        allocator->sets_per_pool *= 1.5;
        if (allocator->sets_per_pool > 4092) {
            allocator->sets_per_pool = 4092;
        }
    }
    return new_pool;
}
VkDescriptorPool descriptor_set_allocator_create_pool(
    ds_allocator* allocator,
    renderer_state* state)
{
    VkDescriptorPoolSize* sizes = dynarray_create(1, sizeof(VkDescriptorPoolSize));
    for (u32 i = 0; i < allocator->pool_size_count; ++i) {
        VkDescriptorPoolSize new_size = {
            .type = allocator->pool_sizes[i].type,
            .descriptorCount = allocator->pool_sizes[i].ratio * allocator->sets_per_pool
        };
        dynarray_push((void**)&sizes, &new_size);
    }
    
    VkDescriptorPoolCreateInfo pool_info = init_descriptor_pool_create_info();
    pool_info.flags = 0;
    pool_info.maxSets = allocator->sets_per_pool;
    pool_info.poolSizeCount = dynarray_length(sizes);
    pool_info.pPoolSizes = sizes;

    VkDescriptorPool new_pool;
    VK_CHECK(vkCreateDescriptorPool(
        state->device.handle,
        &pool_info,
        state->allocator,
        &new_pool));

    // Clean up memory
    dynarray_destroy(sizes);

    return new_pool;
}