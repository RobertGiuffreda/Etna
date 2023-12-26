#pragma once

#include "renderer/src/vk_types.h"

/* NOTE: dsl_builder functions */
dsl_builder descriptor_set_layout_builder_create(void);
void descriptor_set_layout_builder_clear(dsl_builder* builder);
void descriptor_set_layout_builder_destroy(dsl_builder* builder);
void descriptor_set_layout_builder_add_binding(dsl_builder* builder, u32 binding, u32 count, VkDescriptorType type, VkShaderStageFlags stage_flags);
VkDescriptorSetLayout descriptor_set_layout_builder_build(dsl_builder* builder, renderer_state* state);
/* NOTE: END */

/* NOTE: ds_writer functions */
ds_writer descriptor_set_writer_create_initialize(void);
void descriptor_set_writer_initialize(ds_writer* writer);
void descriptor_set_writer_shutdown(ds_writer* writer);

void descriptor_set_writer_write_image(ds_writer* writer, u32 binding, VkImageView view, VkSampler sampler, VkImageLayout layout, VkDescriptorType type);
void descriptor_set_writer_write_buffer(ds_writer* writer, u32 binding, VkBuffer buffer, u64 size, u64 offset, VkDescriptorType type);

void descriptor_set_writer_clear(ds_writer* writer);
void descriptor_set_writer_update_set(ds_writer* writer, VkDescriptorSet set, renderer_state* state);
/* NOTE: END */

/* NOTE: ds_allocator functions */
void descriptor_set_allocator_initialize(ds_allocator* allocator, u32 max_sets, VkDescriptorPoolSize* pool_sizes, renderer_state* state);
void descriptor_set_allocator_create_pool(ds_allocator* allocator, u32 max_sets, renderer_state* state);
void descriptor_set_allocator_clear_descriptor_sets(ds_allocator* allocator, renderer_state* state);
void descriptor_set_allocator_shutdown(ds_allocator* allocator, renderer_state* state);
void descriptor_set_allocator_destroy_pool(ds_allocator* allocator, renderer_state* state);
VkDescriptorSet descriptor_set_allocator_allocate(ds_allocator* allocator, VkDescriptorSetLayout layout, renderer_state* state);
/* NOTE: END */

/* NOTE: ds_allocator_growable */
void descriptor_set_allocator_growable_intialize(ds_allocator_growable* allocator);
void descriptor_set_allocator_growable_clear_pools(ds_allocator_growable* allocator);
void descriptor_set_allocator_growable_destroy_pools(ds_allocator_growable* allocator);
/* NOTE: END */