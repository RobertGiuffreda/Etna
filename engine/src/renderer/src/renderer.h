#pragma once

#include "defines.h"

#include "core/camera.h"
#include "renderer/src/vk_types.h"

typedef struct renderer_state {
    VkInstance instance;
#ifdef _DEBUG
    VkDebugUtilsMessengerEXT debug_messenger;
    PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT;
#endif
    VkAllocationCallbacks* allocator;

    // Window elements
    // Extent of render image decides resolution
    VkExtent3D render_extent;
    VkSurfaceKHR surface;
    // True if window was resized
    b8 swapchain_dirty;

    device device;

    VkSwapchainKHR swapchain;
    VkFormat swapchain_image_format;

    // Determines amount of frames: Length of perframe arrays
    u32 image_count;
    VkExtent3D window_extent;
    VkImage* swapchain_images;
    VkImageView* swapchain_image_views;

    // Image to render to that get copied onto the swapchain image
    image render_image;
    image depth_image;

    // Index into per frame arrays: TODO: Rename to frame_index maybe??
    u32 current_frame; 
    
    // NOTE: Per frame structures/data. Add to separate struct??
    VkSemaphore* swapchain_semaphores;
    VkSemaphore* render_semaphores;

    VkFence* render_fences;
    
    // Going to use the graphics queue for everything for now until I get a
    // better handle on things. 
    VkCommandPool* graphics_pools;
    VkCommandBuffer* main_graphics_command_buffers;

    ds_allocator_growable* frame_allocators;
    // NOTE: Per frame END

    // Immediate command pool & buffer
    // TODO: How does this fit into place with with queues 
    // and multiple queue families 
    VkCommandPool imm_pool;
    VkCommandBuffer imm_buffer;
    VkFence imm_fence;

    void (*immediate_begin)(struct renderer_state* state);
    void (*immediate_end)(struct renderer_state* state);

    // Global descriptor set allocator
    ds_allocator_growable global_ds_allocator;

    shader gradient_shader;
    compute_effect gradient_effect;

    VkDescriptorSet draw_image_descriptor_set;
    VkDescriptorSetLayout draw_image_descriptor_set_layout;
    VkDescriptorSetLayout single_image_descriptor_set_layout;

    // Testing loading data to vertex & index buffer as well as buffer references
    shader mesh_vertex;
    shader mesh_fragment;

    VkPipeline mesh_pipeline;
    VkPipelineLayout mesh_pipeline_layout;

    // TEMP: Until loading a scene instead of meshes
    mesh_asset* meshes;
    // TEMP: END

    // TEMP: Section 4 of guide
    // Default images/textures to use
    image white_image;
    image black_image;
    image grey_image;
    image error_checkerboard_image;

    VkSampler default_sampler_linear;
    VkSampler default_sampler_nearest;
    
    GLTF_MR metal_rough_material;
    buffer material_constants;

    // Scene data
    draw_context main_draw_context;
    GPU_scene_data scene_data;
    material_instance default_data;

    // TEMP:TODO: Have a node allocator that allocates backing nodes
    // and returns references to them.
    // For now have the backing mesh nodes & nodes backed here in memory
    u32 backing_mesh_node_count;
    mesh_node* backing_mesh_nodes;
    u32 backing_node_count;
    node* backing_nodes;
    // TEMP:TODO: END

    // An array of pointers to nodes located in the backing array above
    node** loaded_nodes;

    VkDescriptorSetLayout scene_data_descriptor_set_layout;

    // Buffers for each frame to store scene data
    buffer* scene_data_buffers;
    // TEMP: END

    gpu_mesh_buffers rectangle;

    // TEMP: Guide chapter 5
    camera main_camera;
    // TEMP: END
} renderer_state;

/** Takes a code block in the ... argument and begins recording of a command buffer before the 
 * code block is run and submits it afterwards  
 * @param state The renderer_state pointer
 * @param cmd The variable name of the VkCommandBuffer to be used in the code block
 * @param ... The code block containing the commands to record and submit
 */
#define IMMEDIATE_SUBMIT(state, cmd, ...)       \
do {                                            \
    state->immediate_begin(state);              \
    VkCommandBuffer cmd = state->imm_buffer;    \
    do {                                        \
        __VA_ARGS__;                            \
    } while (0);                                \
    state->immediate_end(state);                \
} while(0);                                     \

#ifdef _DEBUG
// TODO: Move definition to a utility function file
b8 renderer_set_debug_object_name(renderer_state* state, VkObjectType object_type, u64 object_handle, const char* object_name);
#define SET_DEBUG_NAME(render_state, object_type, object_handle, object_name) renderer_set_debug_object_name(render_state, object_type, (u64)(object_handle), object_name)
#elif 
#define SET_DEBUG_NAME()
#endif
