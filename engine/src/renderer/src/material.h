#pragma once

#include "renderer/src/vk_types.h"
#include "renderer/src/shader.h"

/** NOTE:
 * struct to hold the set_layout and the push constant layout of the material pipeline
 * 
 */

typedef struct material_blueprint {
    shader vertex;
    shader fragment;

    set_layout material_set_layout;

    material_pipeline opaque_pipeline;
    material_pipeline transparent_pipeline;

    VkDescriptorSetLayout layout;

    ds_writer writer;
} material_blueprint;

b8 material_blueprint_create(renderer_state* state, const char* vertex_path, const char* fragment_path, material_blueprint* blueprint);
void material_blueprint_destroy(renderer_state* state, material_blueprint* blueprint);

// material_instance material_blueprint_create_instance(
//     renderer_state* state,
//     material_blueprint* blueprint,
//     const mat_resources* resources,
//     ds_allocator_growable* allocator);