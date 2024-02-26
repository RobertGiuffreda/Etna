#pragma once
#include "defines.h"

#include "core/camera.h"
#include "core/clock.h"
#include "math/math_types.h"
#include "renderer/src/vk_types.h"
#include "renderer/src/renderables.h"

#include "resources/resource_private.h"

/** Checklist:
 * Parent child relationship between objects/transforms to make scene graph.
 * (Double Ended Queue / Ring queue) for traversing the scene graph
 * 
 * Information that changes each frame.
 * 
 * 
 * Use vertex offset to differentiate meshes in the vertex buffer so that index buffer can be stored normally
 * 
 * 
 * Image, Material, Surface, Mesh, Object, resource managers
 * 
 * 
 * Features:
 * Create material blueprint from Shader reflection data taking into account bindless design.
 * Sort calls by different blueprints and by transparency: Mesh contains index into surfaces for opaque surface and transparent surfaces.
 * Genuine Instancing - Hardcoded MAX instance amount for each mesh, controlls offsets into transform buffer.
 * Indirect generated on GPU - Compute buffer to cull & sort by material blueprint & instance & generate draw calls.
 * 
 */

typedef struct scene {
    char* name;

    // TEMP: Until bindless has taken over
    u64 vertex_count;
    u64 index_count;
    vertex* vertices;
    u32* indices;

    buffer index_buffer;
    buffer vertex_buffer;
    VkDeviceAddress vb_addr;    // Vertex buffer address

    buffer transform_buffer;
    VkDeviceAddress tb_addr;

    buffer* draw_buffers;       // Perframe
    buffer* scene_uniforms;     // Perframe

    buffer bindless_material_buffer;

    surface_2* surfaces;    // dynarray
    mesh_2* meshes;         // dynarray
    m4s* transforms;        // dynarray
    object* objects;        // dynarray

    u64 surface_count;
    u64 mesh_count;
    u64 transform_count;
    u64 object_count;

    // NOTE: PSOs must implement SET 0 to match this layout & retrieve the information
    VkDescriptorSetLayout scene_layout;
    VkDescriptorSet* scene_sets;
    VkPushConstantRange push_constant;

    // TODO: Material manager bindless & support for multiple PSOs.
    VkPipeline opaque_pipeline;
    VkPipeline transparent_pipeline;
    VkPipelineLayout pipeline_layout;

    VkDescriptorSetLayout material_layout;
    VkDescriptorSet material_set;

    material_2 materials[MAX_MATERIAL_COUNT];
    u32 material_count;
    // TODO: END
    
    VkDescriptorPool bindless_pool;

    // Draw commands for indirect bindless.
    u64 op_draws_count;
    u64 tp_draws_count;

    draw_command* opaque_draws;
    draw_command* transparent_draws;
    // TEMP: END

    camera cam;
    scene_data data;

    // TODO: Update to current GPU Driven architecture
    image_manager* image_bank;
    material_manager* material_bank;
    mesh_manager* mesh_bank;
    // TODO: END

    // TODO: Delete node structure and replace with transform hierarchy
    node** top_nodes;
    u32 top_node_count;

    node* nodes;
    u32 node_count;
    mesh_node* mesh_nodes;
    u32 mesh_node_count;
    // TODO: END

    // TODO: Put all sampler combos into renderer_state as
    // defaults to avoid having them here. 
    u32 sampler_count;
    VkSampler* samplers;
    // TODO: END

    // TODO: Put into material manager and suballocate??
    buffer material_buffer;
    // TODO: END

    renderer_state* state;
} scene;

// TEMP: Bindless testing
b8 scene_material_set_instance(scene* scene, material_pass pass_type, struct bindless_material_resources* resources);
material_2 scene_material_get_instance(scene* scene, material_id id);

void scene_image_set(scene* scene, u32 img_id, u32 sampler_id);
// TEMP: END