#pragma once
#include "defines.h"
#include "scene/scene_types.h"

#include "core/camera.h"
#include "core/clock.h"

#include "resources/resource_private.h"
#include "resources/material.h"

/** TODO:
 * Clean up loading from the import payload
 * 
 * Parent child relationship between objects/transforms to make scene graph.
 * (Double Ended Queue / Ring queue) for traversing the scene graph
 */

typedef struct scene {
    const char* name;

    // NOTE: Data that will be serialized and loaded
    camera cam;
    scene_data data;

    vertex* vertices;       // dynarray
    u32* indices;           // dynarray
    m4s* transforms;        // dynarray
    geometry* geometries;   // dynarray
    object* objects;        // dynarray
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

    // NOTE: Render image, depth image
    VkExtent3D render_extent;
    image render_image;
    image depth_image;

    // NOTE: Index into draws_buffer for shadow_draws is a scene_data struct member
    buffer shadow_draws;                    // Draw command buffer for indirect drawing
    image shadow_map;                       // Depth map on shadow pass, sampler2D on lighting pass
    VkSampler shadow_map_sampler;
    VkPipeline shadow_draw_gen_pipeline;    // Uses draw_gen_layout as VkPipelineLayout
    VkPipeline shadow_pipeline;             // Pipeline to render to the shadow map

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

    // TODO: Clean up implementation
    mat_pipe_config* mat_pipe_configs;
    // TODO: image2D_config* image_configs;

    u32 mat_pipe_count;
    mat_pipe* mat_pipes;
    // TODO: END

    image_manager* image_bank;

    u32 sampler_count;
    VkSampler* samplers;

    import_payload payload;
    // TODO: END

    renderer_state* state;
} scene;