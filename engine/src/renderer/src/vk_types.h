#pragma once

#include "defines.h"
#include "core/asserts.h"

#include "math/math_types.h"

#include <vulkan/vulkan.h>

// TODO: This file should be split into an actual vk_types.h file and a
// renderer.h file. Where the current renderer.h file is being used as a 
// rendererAPI.h file. 

#define VK_CHECK(expr) { ETASSERT((expr) == VK_SUCCESS); }

/* NOTE: Descriptor Set Abstraction structs */
// TODO: Move these structs back to their respective header files
// and just #include <vulkan/vulkan.h>.

typedef struct descriptor_set_layout_builder {
    // Dynarray
    VkDescriptorSetLayoutBinding* bindings;
} dsl_builder;

// Descriptor Set writer. Stores descriptor set writes so they can
// be applied all at once in the command vkUpdateDescriptorSets 
typedef struct descriptor_set_writer {
    VkDescriptorImageInfo* image_infos;
    VkDescriptorBufferInfo* buffer_infos;

    VkWriteDescriptorSet* writes;
} ds_writer;

typedef struct descriptor_set_allocator {
    // Dynarray created from hardcoded array(for now)
    VkDescriptorPoolSize* pool_sizes;
    VkDescriptorPool pool;
} ds_allocator;

// Growable descriptor allocator types
typedef struct pool_size_ratio {
    VkDescriptorType type;
    float ratio;
} pool_size_ratio;

typedef struct descriptor_set_allocator_growable {
    pool_size_ratio* pool_sizes;
    VkDescriptorPool* ready_pools;
    VkDescriptorPool* full_pools;
    u32 sets_per_pool;
} ds_allocator_growable;
/* NOTE: END */

// TEMP: Until my vulkan memory management is implemented
// TODO: Store memory information for when implementing memory management
typedef struct image {
    VkImage handle;
    VkImageView view;
    VkDeviceMemory memory;

    VkImageType type;
    VkExtent3D extent;
    VkFormat format;
    VkImageAspectFlags aspects;
} image;

typedef struct buffer {
    VkBuffer handle;
    VkDeviceMemory memory;
    u64 size;
} buffer;
// TEMP: END

// NOTE: vkguide.dev structs
typedef struct gpu_mesh_buffers {
    buffer index_buffer;
    buffer vertex_buffer;
    VkDeviceAddress vertex_buffer_address;
} gpu_mesh_buffers;

typedef struct gpu_draw_push_constants {
    m4s world_matrix;
    VkDeviceAddress vertex_buffer;
} gpu_draw_push_constants;
// NOTE: END

typedef struct GPU_scene_data {
    m4s view;
    m4s proj;
    m4s viewproj;
    v4s ambient_color;
    v4s sunlight_direction; // w for sun power
    v4s sunlight_color;
    v4s padding;            // 256 byte minimum on my GPU
} GPU_scene_data;

// TODO:TEMP: Hacky c code mimicking C++ OOP from vkguide.dev. 
// This is until something more scalable or better for c is 
// ironed out. 
typedef enum material_pass {
    MATERIAL_PASS_MAIN_COLOR,
    MATERIAL_PASS_TRANSPARENT,
    MATERIAL_PASS_OTHER
} material_pass;

typedef struct material_pipeline {
    VkPipeline pipeline;
    VkPipelineLayout layout;
} material_pipeline;

typedef struct material_instance {
    material_pipeline* pipeline;
    VkDescriptorSet material_set;
    material_pass pass_type;
} material_instance;

typedef struct GLTF_material {
    material_instance data;
} GLTF_material;

typedef struct geo_surface {
    u32 start_index;
    u32 count;

    // Not responsible for freeing
    GLTF_material* material;
} geo_surface;

typedef struct mesh_asset {
    const char* name;

    // Dynarray
    geo_surface* surfaces;
    gpu_mesh_buffers mesh_buffers;
} mesh_asset;

typedef struct render_object {
    u32 index_count;
    u32 first_index;
    VkBuffer index_buffer;

    material_instance* material;

    m4s transform;
    VkDeviceAddress vertex_buffer_address;
} render_object;

typedef struct draw_context {
    // Dynarray
    render_object* opaque_surfaces;
} draw_context;

// TODO: Should not be visible
typedef struct renderable_virtual_table {
    void (*draw)(void* self, const m4s top_matrix, draw_context* ctx);
    void (*destroy)(void* self);
} renderable_vt;

// TODO: This should be opaque type
typedef struct renderable {
    void* self;
    renderable_vt* vt;
} renderable;

// TODO: Should not be visible
typedef struct node_virtual_table {
    void (*draw)(void* self, const m4s top_matrix, draw_context* ctx);
    void (*destroy)(void* self);
} node_vt;

typedef struct node {
    // Extends renderable
    renderable renderable;

    // Polymorphism data
    void* self;
    node_vt* vt;

    // Actual struct node data
    struct node* parent;
    struct node** children;
    m4s local_transform;
    m4s world_transform;
} node;

typedef struct mesh_node {
    // Extends node
    struct node base;

    // Pointer as mesh can be shared between multiple nodes
    mesh_asset* mesh;
} mesh_node;

typedef struct GLTFMetallic_Roughness {
    material_pipeline opaque_pipeline;
    material_pipeline transparent_pipeline;

    VkDescriptorSetLayout material_layout;

    struct material_constants {
        v4s color_factors;
        v4s metal_rough_factors;
        v4s padding[14];
    };

    struct material_resources {
        image color_image;
        VkSampler color_sampler;

        image metal_rough_image;
        VkSampler metal_rough_sampler;

        VkBuffer data_buffer;
        u32 data_buffer_offset;
    };

    ds_writer writer;
} GLTF_MR;

// TODO:TEMP: END

typedef struct binding {
    u32 binding;
    VkDescriptorType descriptor_type;
    u32 count;
    struct {
        VkImageType dim;
        VkFormat format;
    } image;
} binding;

typedef struct set {
    u32 set;
    // Dynarray
    u32 binding_count;
    binding* _bindings;
} set;

typedef struct shader {
    VkShaderModule module;
    VkShaderStageFlagBits stage;
    char* entry_point;
    u32 set_count;
    set* _sets;

    // TODO: Get information about push constants
    u32 push_constant_count;
    struct {
        u32 temp;
    }*push_constants;

    // TODO: Add type information maybe
    u32 input_count;
    struct {
        u32 location;
        VkFormat format;
    }*inputs;

    // TODO: Add type information maybe
    u32 output_count;
    struct {
        u32 location;
        VkFormat format;
    }*outputs;
} shader;

/** TEMP: & TODO:
 * This section involving compute effects is a bit of a mess and
 * is temporary. This will be here until a material system is thought out and 
 * implemented.
 * 
 * Compute push constants are TEMP: and will be replaced with more robust system I think:
 * Compute shader post processing effects for now all share compute push constants to simplify things
 */
// NOTE: vkguide.dev structs
typedef struct compute_push_constants {
    v4s data1;
    v4s data2;
    v4s data3;
    v4s data4;
} compute_push_constants;

typedef struct compute_effect {
    const char* name;

    VkPipeline pipeline;
    VkPipelineLayout layout;

    compute_push_constants data;
} compute_effect;
// NOTE: END

/* TEMP: & TODO: END */

typedef struct device {
    VkDevice handle;
    VkPhysicalDevice gpu;
    VkPhysicalDeviceLimits gpu_limits;
    VkPhysicalDeviceMemoryProperties gpu_memory_props;

    // TODO: Better name which reflects that it is a queue family index
    // Not just a queue index
    i32 graphics_queue_index;
    i32 compute_queue_index;
    i32 transfer_queue_index;
    i32 present_queue_index;

    VkQueue graphics_queue;
    VkQueue compute_queue;
    VkQueue transfer_queue;
    VkQueue present_queue;
} device;

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
    i32 width;
    i32 height;
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

    // TODO: Make this a hash_map. For now, just an array of pointers
    node** loaded_nodes;

    VkDescriptorSetLayout scene_data_descriptor_set_layout;

    // Buffers for each frame to store scene data
    buffer* scene_data_buffers;
    // TEMP: END

    gpu_mesh_buffers rectangle;
} renderer_state;

#define IMM_CMD_BUFFER(state) state->imm_buffer

/** 
 * Hacky macro for encapsulating code, ... portion, inside the 
 * vulkan start and end function for the command buffer designated for 
 * immediate submissions
 */
#define IMMEDIATE_SUBMIT(state, ...)          \
do {                                            \
    state->immediate_begin(state);              \
    do {                                        \
        __VA_ARGS__;                            \
    } while (0);                                \
    state->immediate_end(state);                \
} while(0);                                     \

#ifdef _DEBUG
// TODO: Move definition to a utility function file
b8 renderer_set_debug_object_name(renderer_state* state, VkObjectType object_type, u64 object_handle, const char* object_name);
#define SET_DEBUG_NAME(render_state, object_type, object_handle, object_name) renderer_set_debug_object_name(render_state, object_type, (u64)object_handle, object_name)
#elif 
#define SET_DEBUG_NAME()
#endif
