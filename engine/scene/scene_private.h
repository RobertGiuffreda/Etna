#pragma once
#include "defines.h"

#include "core/camera.h"
#include "core/clock.h"
#include "math/math_types.h"
#include "renderer/src/vk_types.h"

#include "resources/resource_private.h"

/** NOTE:
 * Refactors:
 * Use vertex offset to differentiate meshes in the vertex buffer so that index buffers can be stored normally
 * Walk scene graph updating information
 * 
 * Features:
 * Create material blueprint from Shader reflection data taking into account bindless design.
 * Sort calls by different blueprints and by transparency: Mesh contains index into surfaces for opaque surface and transparent surfaces.
 * Genuine Instancing - Hardcoded MAX instance amount for each mesh, controlls offsets into transform buffer.
 * Indirect generated on GPU - Compute buffer to cull & sort by material blueprint & instance & generate draw calls.
 * 
 * Eventual:
 * Parent child relationship between objects/transforms to make scene graph.
 * (Double Ended Queue / Ring queue) for traversing the scene graph
 * 
 */

typedef struct scene {
    char* name;

    // NOTE: Data representation on cpu
    camera cam;
    scene_data data;

    u64 vertex_count;
    vertex* vertices;

    u64 index_count;
    u32* indices;

    u64 surface_count;
    surface* surfaces;       // dynarray

    u64 mesh_count;
    mesh* meshes;            // dynarray
    
    u64 transform_count;
    m4s* transforms;         // dynarray
    // NOTE: END

    // NOTE: GPU structures to generate draw calls.
    geometry* geometries;       // All geometries for testing atm
    object* objects;            // All objects for testing atm
    // NOTE: END

    // NOTE: GPU Memory Buffers
    buffer vertex_buffer;
    buffer index_buffer;
    buffer transform_buffer;

    buffer scene_uniforms;      // Per Frame Uniform data
    buffer object_buffer;       // Contains Object information used to generate draws
    buffer geometry_buffer;

    // TODO: Rename to suit multiple material pipelines
    buffer count_buffer;        // Holds the counts for each pipeline draw indirect
    buffer draw_buffer;         // Holds pointers to each material pipelines draw buffers
    // NOTE: END
    
    VkFence* render_fences;
    VkCommandPool* graphics_pools;
    VkCommandBuffer* graphics_command_buffers;

    VkDescriptorPool descriptor_pool;

    VkPipeline draw_gen_pipeline;
    VkPipelineLayout draw_gen_layout;
    
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

    buffer material_draws_buffer;
    VkDeviceAddress mat_draws_addr;

    buffer material_buffer;
    material materials[MAX_MATERIAL_COUNT];
    u32 material_count;
    // TODO: END
    
    // TODO: Update to current GPU Driven architecture
    image_manager* image_bank;
    u32 sampler_count;
    VkSampler* samplers;
    // TODO: END

    renderer_state* state;
} scene;

// TEMP: Managing resources is a mess
b8 scene_material_set_instance(scene* scene, material_pass pass_type, struct material_resources* resources);
material scene_material_get_instance(scene* scene, material_id id);

void scene_texture_set(scene* scene, u32 img_id, u32 sampler_id);
// TEMP: END