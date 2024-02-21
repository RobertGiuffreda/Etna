#pragma once

#include "renderer/src/vk_types.h"
#include "renderer/src/shader.h"

typedef struct material_resource {
    u32 binding;
    union {
        struct {
            VkSampler sampler;
            VkImageView view;
        };
        struct {
            VkBuffer buffer;
            u64 offset;
        };
    };
} material_resource;

// typedef struct binding_info {
//     u32 binding;
//     u32 count;
//     descriptor_type type;
//     union {
//         VkImageLayout image_layout;
//         u64 buffer_range;
//     };
// } binding_info;

typedef struct material_blueprint {
    material_pipeline opaque_pipeline;
    material_pipeline transparent_pipeline;
    VkDescriptorSetLayout ds_layout;

    shader vertex;
    shader fragment;

    u32 binding_count;
    // NOTE: Only needed for memory allocation metrics in free function
    u32 image_count;
    u32 buffer_count;
    // NOTE: END

    VkDescriptorImageInfo* image_infos;
    VkDescriptorBufferInfo* buffer_infos;
    VkWriteDescriptorSet* writes;
} material_blueprint;

b8 material_blueprint_create(renderer_state* state, const char* vertex_path, const char* fragment_path, material_blueprint* blueprint);

void material_blueprint_destroy(renderer_state* state, material_blueprint* blueprint);

material_instance material_blueprint_create_instance(
    renderer_state* state,
    material_blueprint* blueprint,
    material_pass pass,
    const material_resource* resources,
    ds_allocator* allocator);