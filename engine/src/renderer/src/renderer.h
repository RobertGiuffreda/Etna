#pragma once

#include "defines.h"

// TEMP: Camera should belong to the scene being rendered. Have camera as a scene member
#include "core/camera.h"
// TEMP: END

#include "renderer/src/vk_types.h"
#include "renderer/src/renderables.h"
#include "renderer/src/shader.h"

#include "scene/scene_private.h"

// TODO: Remove, this is deprecated
typedef struct loaded_gltf {
    char* name;

    // The loaded_gltf struct currently stores the backing memory for
    // it's meshes, images, materials, and nodes.
    mesh* meshes;
    u32 mesh_count;
    image* images;
    u32 image_count;
    material* materials;
    u32 material_count;

    mesh_node* mesh_nodes;
    u32 mesh_node_count;
    node* nodes;
    u32 node_count;

    VkSampler* samplers;
    u32 sampler_count;

    // Dynarray of top level node pointers/references
    node** top_nodes;
    u32 top_node_count;

    ds_allocator descriptor_allocator;

    buffer material_data_buffer;

    renderer_state* render_state;
} loaded_gltf;

// TODO: Have array of these instead of separate arrays
typedef struct frame_data {
    VkSemaphore swapchain_semaphore;
    VkSemaphore render_semaphore;
    VkFence render_fence;
    VkCommandPool command_pool;
    VkCommandBuffer command_buffer;
    ds_allocator ds_allocator;
    buffer scene_data_buffer;
} frame_data;

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

    VkExtent3D window_extent;
    
    // True if window was resized
    b8 swapchain_dirty;
    // NOTE: END

    // NOTE: Main render images and attachments
    VkExtent3D render_extent;
    image render_image;
    image depth_image;
    // NOTE: END

    u32 frame_index;
    
    // NOTE: Per frame structures/data
    VkSemaphore* swapchain_semaphores;
    VkSemaphore* render_semaphores;

    VkFence* render_fences;
    
    // Graphics queue for everything until I get a handle vulkan.
    VkCommandPool* graphics_pools;
    VkCommandBuffer* main_graphics_command_buffers;

    // Descriptor Set allocator for allocating per frame descriptors
    ds_allocator* frame_allocators;

    buffer* scene_data;
    // NOTE: Per frame END

    VkCommandPool imm_pool;
    VkCommandBuffer imm_buffer;
    VkFence imm_fence;

    ds_allocator global_ds_allocator;

    // NOTE: Compute effect members
    shader gradient_shader;
    compute_effect gradient_effect;

    VkDescriptorSetLayout draw_image_descriptor_set_layout;
    VkDescriptorSet draw_image_descriptor_set;
    // NOTE: END

    // NOTE: Defaults
    image white_image;
    image black_image;
    image grey_image;
    image error_image;

    VkSampler default_sampler_linear;
    VkSampler default_sampler_nearest;
    
    GLTF_MR metal_rough_material;

    material_instance default_material_instance;
    buffer default_material_constants;

    VkDescriptorSetLayout scene_data_descriptor_set_layout;
    scene _scene;
    
    draw_context main_draw_context;
} renderer_state;

// TODO: Setup debug names for vulkan objects
#ifdef _DEBUG
b8 renderer_set_debug_object_name(renderer_state* state, VkObjectType object_type, u64 object_handle, const char* object_name);
#define SET_DEBUG_NAME(render_state, object_type, object_handle, object_name) renderer_set_debug_object_name(render_state, object_type, (u64)(object_handle), object_name)
#elif 
#define SET_DEBUG_NAME()
#endif
