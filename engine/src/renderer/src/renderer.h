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

    device device;

    // NOTE: Swapchain & window data
    VkSwapchainKHR swapchain;
    VkImage* swapchain_images;
    VkImageView* swapchain_image_views;
    VkFormat swapchain_image_format;

    // Determines amount of frames: Length of perframe arrays
    u32 image_count;

    VkSurfaceKHR surface;
    // TODO: Rename swapchain extent??
    VkExtent3D window_extent;
    
    // True if window was resized
    b8 swapchain_dirty;
    // NOTE: END

    // NOTE: Main render images and attachments
    VkExtent3D render_extent;
    image render_image;
    image depth_image;
    // NOTE: END

    // TODO: Rename to frame_index maybe??
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

    // Buffers for each frame to store scene data. Idk if this should be per frame
    buffer* scene_data_buffers;
    // NOTE: Per frame END

    // NOTE: Immediate command pool & buffer
    // TODO: How does this fit into place with with queues 
    // and multiple queue families 
    VkCommandPool imm_pool;
    VkCommandBuffer imm_buffer;
    VkFence imm_fence;

    void (*immediate_begin)(struct renderer_state* state);
    void (*immediate_end)(struct renderer_state* state);
    // NOTE: END

    // Global descriptor set allocator
    ds_allocator_growable global_ds_allocator;

    // NOTE: Compute effect members
    shader gradient_shader;
    compute_effect gradient_effect;

    VkDescriptorSetLayout draw_image_descriptor_set_layout;
    VkDescriptorSet draw_image_descriptor_set;
    // NOTE: END

    // NOTE: Default images, samplers, materials
    image white_image;
    image black_image;
    image grey_image;
    image error_checkerboard_image;

    VkSampler default_sampler_linear;
    VkSampler default_sampler_nearest;
    
    GLTF_MR metal_rough_material;

    material_instance default_material_instance;
    buffer default_material_constants;
    // NOTE: END

    // Scene data. Per frame buffers for passing scene data to shaders above 
    GPU_scene_data scene_data;
    VkDescriptorSetLayout scene_data_descriptor_set_layout;
    
    draw_context main_draw_context;

    loaded_gltf _gltf;
    camera main_camera;

    // TEMP: Here in case of testing
    gpu_mesh_buffers rectangle;
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
b8 renderer_set_debug_object_name(renderer_state* state, VkObjectType object_type, u64 object_handle, const char* object_name);
#define SET_DEBUG_NAME(render_state, object_type, object_handle, object_name) renderer_set_debug_object_name(render_state, object_type, (u64)(object_handle), object_name)
#elif 
#define SET_DEBUG_NAME()
#endif
