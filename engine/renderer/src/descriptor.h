#pragma once

#include "renderer/src/vk_types.h"

dsl_builder descriptor_set_layout_builder_create(void);
void descriptor_set_layout_builder_clear(dsl_builder* builder);
void descriptor_set_layout_builder_set_flags(dsl_builder* builder, VkDescriptorSetLayoutCreateFlags flags);
void descriptor_set_layout_builder_destroy(dsl_builder* builder);
void descriptor_set_layout_builder_add_binding(dsl_builder* builder, u32 binding, u32 count, VkDescriptorType type, VkShaderStageFlags stage_flags);
VkDescriptorSetLayout descriptor_set_layout_builder_build(dsl_builder* builder, renderer_state* state);

ds_writer descriptor_set_writer_create_initialize(void);
void descriptor_set_writer_initialize(ds_writer* writer);
void descriptor_set_writer_shutdown(ds_writer* writer);

void descriptor_set_writer_write_image(ds_writer* writer, u32 binding, VkImageView view, VkSampler sampler, VkImageLayout layout, VkDescriptorType type);
void descriptor_set_writer_write_buffer(ds_writer* writer, u32 binding, VkBuffer buffer, u64 size, u64 offset, VkDescriptorType type);

void descriptor_set_writer_clear(ds_writer* writer);
void descriptor_set_writer_update_set(ds_writer* writer, VkDescriptorSet set, renderer_state* state);

void descriptor_set_allocator_initialize(ds_allocator* allocator, u32 initial_sets, u32 pool_size_count, pool_size_ratio* pool_sizes, renderer_state* state);
void descriptor_set_allocator_shutdown(ds_allocator* allocator, renderer_state* state);
void descriptor_set_allocator_clear_pools(ds_allocator* allocator, renderer_state* state);
void descriptor_set_allocator_destroy_pools(ds_allocator* allocator, renderer_state* state);
VkDescriptorSet descriptor_set_allocator_allocate(ds_allocator* allocator, VkDescriptorSetLayout layout, renderer_state* state);

VkDescriptorPool descriptor_set_allocator_get_pool(ds_allocator* allocator, renderer_state* state);
VkDescriptorPool descriptor_set_allocator_create_pool(ds_allocator* allocator, renderer_state* state);