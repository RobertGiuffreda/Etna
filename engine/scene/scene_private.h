#pragma once
#include "defines.h"
#include "scene/scene_types.h"

#include "core/camera.h"
#include "core/clock.h"

#include "resources/resource_private.h"
#include "resources/material.h"

/** TODO:
 * Multiple material shaders with different pipelines
 * Use vertex offset to differentiate meshes in the vertex buffer so that index buffers can be stored normally
 * Use the 
 * 
 * 
 * Parent child relationship between objects/transforms to make scene graph.
 * (Double Ended Queue / Ring queue) for traversing the scene graph
 * 
 * Keep in mind:
 * Genuine Instancing - Hardcoded MAX instance amount for each mesh, controlls offsets into transform buffer.
 */

static const mat_pipe_config scene_mat_configs[MAT_PIPE_MAX] = {
    [MAT_PIPE_METAL_ROUGH] = {
        .vert_path = "assets/shaders/blinn_mr.vert.spv.opt",
        .frag_path = "assets/shaders/blinn_mr.frag.spv.opt",
        .inst_size = sizeof(blinn_mr_instance),
        .transparent = false,
    },
    [MAT_PIPE_METAL_ROUGH_TRANSPARENT] = {
        .vert_path = "assets/shaders/blinn_mr_tp.vert.spv.opt",
        .frag_path = "assets/shaders/blinn_mr_tp.frag.spv.opt",
        .inst_size = sizeof(blinn_mr_instance),
        .transparent = true,
    },
};

typedef struct scene {
    char* name;

    // NOTE: Data representation on cpu
    camera cam;
    scene_data data;

    u64 vertex_count;
    vertex* vertices;       // dynarray

    u64 index_count;
    u32* indices;           // dynarray

    u64 surface_count;
    surface* surfaces;      // dynarray

    u64 mesh_count;
    mesh* meshes;           // dynarray
    
    u64 transform_count;
    m4s* transforms;        // dynarray
    // NOTE: END

    // NOTE: GPU structures to generate draw calls.
    geometry* geometries;
    object* objects;
    // NOTE: END

    // NOTE: GPU Memory Buffers
    buffer vertex_buffer;
    buffer index_buffer;
    buffer transform_buffer;

    buffer scene_uniforms;      // Per Frame Uniform data
    buffer object_buffer;       // Contains Object information used to generate draws
    buffer geometry_buffer;

    buffer counts_buffer;        // Holds the counts for each pipeline draw indirect
    buffer draws_buffer;         // Holds pointers to each material pipelines draw buffers
    // NOTE: END
    
    // NOTE: Per frame rendering primitives
    VkFence* render_fences;
    VkCommandPool* graphics_pools;
    VkCommandBuffer* graphics_command_buffers;

    VkDescriptorPool descriptor_pool;

    VkPipeline draw_gen_pipeline;
    VkPipelineLayout draw_gen_layout;
    
    // NOTE: PSOs must implement SET 0 to match this layout & retrieve the information
    VkDescriptorSetLayout scene_set_layout;
    VkDescriptorSet scene_set;

    // NOTE: Currenly material pipelines are all hardcoded to
    // To have the same descriptor set layout and pipeline layouts
    VkDescriptorSetLayout mat_set_layout;
    VkPipelineLayout mat_pipeline_layout;

    mat_pipe materials[MAT_PIPE_MAX];

    // TODO: Update to current GPU Driven architecture
    image_manager* image_bank;
    u32 sampler_count;
    VkSampler* samplers;
    // TODO: END

    renderer_state* state;
} scene;