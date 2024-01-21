#pragma once

#include "renderer/src/vk_types.h"
#include "renderer/src/shader.h"

typedef struct material_blueprint {
    shader vertex;
    shader fragment;

    material_pipeline opaque_pipeline;
    material_pipeline transparent_pipeline;

    VkDescriptorSetLayout ds_layout;

    ds_writer writer;
} material_blueprint;

b8 material_blueprint_create(renderer_state* state, const char* vertex_path, const char* fragment_path, material_blueprint* blueprint);
void material_blueprint_destroy(renderer_state* state, material_blueprint* blueprint);

/** NOTE:
 * This function relies on struct material_constants
 * and struct material_resources in order to write to
 * the allocated VkDescriptorSet object.
 * These structs are hardcoded to match the values in 
 * the input_structures.glsl file that gets included 
 * into the current mesh_mat.vert and mesh_mat.frag files.
 */
material_instance material_blueprint_create_instance(
    renderer_state* state,
    material_blueprint* blueprint,
    material_pass pass,
    const struct material_resources* resources,
    ds_allocator_growable* allocator);