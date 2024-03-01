#pragma once
#include "defines.h"

#include "core/camera.h"
#include "core/clock.h"
#include "math/math_types.h"
#include "renderer/src/vk_types.h"
#include "renderer/src/renderables.h"

#include "resources/resource_private.h"

/** NOTE:
 * Refactors:
 * Parent child relationship between objects/transforms to make scene graph.
 * (Double Ended Queue / Ring queue) for traversing the scene graph
 * 
 * Information that changes each frame:
 * Transforms, Scene Objects, 
 * 
 * Use vertex offset to differentiate meshes in the vertex buffer so that index buffer can be stored normally
 * 
 * struct object {
 *     u32 geo_id;
 *     u32 transform_id;
 *     u32 material_id;
 * };
 * 
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

    buffer scene_uniforms;
    buffer object_buffer;
    buffer count_buffer;
    buffer draw_buffer;

    buffer bindless_material_buffer;

    surface* surfaces;      // dynarray
    mesh* meshes;           // dynarray
    m4s* transforms;        // dynarray
    gpu_obj* op_objects;     // dynarray
    gpu_obj* tp_objects;     // dynarray

    u64 surface_count;
    u64 mesh_count;
    u64 transform_count;
    u64 op_object_count;
    u64 tp_object_count;
    
    VkDescriptorPool descriptor_pool;

    // TODO: Compute shader pipeline encapsulation
    VkPipeline draw_gen_pipeline;
    VkPipelineLayout draw_gen_layout;
    // TODO: END
    
    // NOTE: PSOs must implement SET 0 to match this layout & retrieve the information
    VkDescriptorSetLayout scene_set_layout;
    VkDescriptorSet scene_set;
    VkPushConstantRange push_constant;

    // TODO: Material manager bindless & support for multiple PSOs.
    VkPipeline opaque_pipeline;
    VkPipeline transparent_pipeline;
    VkPipelineLayout pipeline_layout;

    VkDescriptorSetLayout material_layout;
    VkDescriptorSet material_set;

    material materials[MAX_MATERIAL_COUNT];
    u32 material_count;
    // TODO: END
    

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
material scene_material_get_instance(scene* scene, material_id id);

void scene_image_set(scene* scene, u32 img_id, u32 sampler_id);
// TEMP: END