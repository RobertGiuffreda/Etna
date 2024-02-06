#pragma once

#include "defines.h"
#include "core/asserts.h"

#include "math/math_types.h"
#include "renderer/renderer_types.h"
#include "resources/resource_types.h"
#define VK_ENABLE_BETA_EXTENSIONS
#include <vulkan/vulkan.h>

#define VK_CHECK(expr) { ETASSERT((expr) == VK_SUCCESS); }

// TEMP: Until descriptor set management refactor
typedef struct descriptor_set_layout_builder {
    VkDescriptorSetLayoutBinding* bindings;  // Dynarray
} dsl_builder;

typedef struct descriptor_set_writer {
    VkDescriptorImageInfo* image_infos;
    VkDescriptorBufferInfo* buffer_infos;

    VkWriteDescriptorSet* writes;
} ds_writer;

typedef struct pool_size_ratio {
    VkDescriptorType type;
    float ratio;
} pool_size_ratio;

typedef struct descriptor_set_allocator {
    pool_size_ratio* pool_sizes;
    u32 pool_size_count;

    VkDescriptorPool* ready_pools;  // Dynarray
    VkDescriptorPool* full_pools;   // Dynarray
    u32 sets_per_pool;
} ds_allocator;
// TEMP: END

// TEMP: Until my vulkan memory management is implemented
typedef struct image {
    u32 id;
    char* name;

    VkImage handle;
    VkImageView view;
    VkDeviceMemory memory;

    VkExtent3D extent;
    VkImageType type;
    VkFormat format;
    VkImageAspectFlags aspects;
} image;

typedef struct buffer {
    char* name;
    VkBuffer handle;
    VkDeviceMemory memory;
    u64 size;
} buffer;
// TEMP: END

typedef struct mesh_buffers {
    buffer index_buffer;
    buffer vertex_buffer;
    VkDeviceAddress vertex_buffer_address;
} mesh_buffers;

typedef struct gpu_draw_push_constants {
    m4s world_matrix;
    VkDeviceAddress vertex_buffer;
} gpu_draw_push_constants;

typedef struct scene_data {
    m4s view;
    m4s proj;
    m4s viewproj;
    v4s ambient_color;
    v4s light_color;
    v4s light_position;
    v4s view_pos;
} scene_data;

typedef struct material_pipeline {
    VkPipeline pipeline;
    VkPipelineLayout layout;
} material_pipeline;

typedef struct material_instance {
    material_pipeline* pipeline;
    VkDescriptorSet material_set;
    material_pass pass_type;
} material_instance;

// TODO: Make material_constants & material_resources generic structs to pass generic information
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

// Default material type as importing GLTF file's is 
// the only thing supported at the moment
typedef struct GLTF_MR {
    material_pipeline opaque_pipeline;
    material_pipeline transparent_pipeline;

    VkDescriptorSetLayout material_layout;

    ds_writer writer;
} GLTF_MR;
// TODO: END

typedef struct material {
    u32 id;
    char* name;
    material_instance data;
} material;

typedef struct surface {
    u32 start_index;
    u32 index_count;

    // Reference
    material* material;
} surface;

typedef struct mesh {
    u32 id;
    char* name;

    // TODO: Stop using dynamic array here
    surface* surfaces;  // Dynarray
    mesh_buffers buffers;
} mesh;

typedef struct render_object {
    char* mesh_name;
    char* material_name;

    u32 index_count;
    u32 first_index;
    VkBuffer index_buffer;

    material_instance* material;

    m4s transform;
    VkDeviceAddress vertex_buffer_address;
} render_object;

typedef struct draw_context {
    // Dynarrays
    render_object* opaque_surfaces;
    render_object* transparent_surfaces;
} draw_context;

/** TEMP:TODO:
 * This section involving compute effects is a bit of a mess and is temporary.
 * 
 * The compute_push_constants struct is TEMP: and will be replaced with more robust system for 
 * post processing. Compute shader post processing effects all share compute_push_constants structure 
 * to simplify things at the moment.
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
// TEMP:TODO: END

typedef struct device {
    VkDevice handle;
    VkPhysicalDevice gpu;
    VkPhysicalDeviceLimits gpu_limits;
    VkPhysicalDeviceMemoryProperties gpu_memory_props;

    // NOTE: _qfi - queue family index
    i32 graphics_qfi;
    i32 compute_qfi;
    i32 transfer_qfi;
    i32 present_qfi;

    // Single queue per family for now
    VkQueue graphics_queue;
    VkQueue compute_queue;
    VkQueue transfer_queue;
    VkQueue present_queue;
} device;