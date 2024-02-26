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
    VkDescriptorSetLayoutCreateInfo layout_info;
    VkDescriptorSetLayoutBinding* binding_infos;  // Dynarray
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

// TEMP: Refactor this
typedef struct mesh_buffers {
    buffer index_buffer;
    buffer vertex_buffer;
    VkDeviceAddress vertex_buffer_address;
} mesh_buffers;

// NOTE: Moving away from this when possible
// Push constants are used by the engine to send the 
// vertex buffer address and model matrix for each draw call
// So its hard coded for now.
typedef struct gpu_draw_push_constants {
    m4s render_matrix;
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

typedef struct draw_command {
    VkDrawIndexedIndirectCommand draw;
    u32 material_instance_id;
    u32 transform_offset;
    u32 padding;
} draw_command;

// TODO: Remove material pipeline and just have 
// VkPipeline & VkPipelineLayout in material_instance & in material blueprint
typedef struct material_pipeline {
    VkPipeline pipeline;
    VkPipelineLayout layout;
} material_pipeline;

typedef struct material_instance {
    material_pipeline* pipeline;
    VkDescriptorSet material_set;
    material_pass pass_type;
} material_instance;

// TODO: Remove the padding and handle 
// the alignment for binding myself 
struct GLTF_MR_constants {
    v4s color_factors;
    v4s metal_rough_factors;
    v4s padding[14];
};

struct GLTF_MR_material_resources {
    image color_image;
    VkSampler color_sampler;

    image metal_rough_image;
    VkSampler metal_rough_sampler;

    VkBuffer data_buffer;
    u32 data_buffer_offset;
};

struct bindless_constants {
    v4s color;
    v4s mr;
    u32 color_id;
    u32 mr_id;
    u32 padd1;
    u32 padd2;
    v4s padding[13];
};

struct bindless_material_resources {
    VkBuffer data_buff;
    u32 data_buff_offset;
};

// Default material type as importing GLTF file's is 
// the only thing supported at the moment
typedef struct GLTF_MR {
    material_pipeline opaque_pipeline;
    material_pipeline transparent_pipeline;

    VkDescriptorSetLayout material_layout;

    // TODO: ds_writer support descriptor count > 1
    ds_writer writer;
} GLTF_MR;

// Move material pass into this
typedef struct material {
    char* name;
    u32 id;
    material_instance instance;
} material;

typedef struct surface {
    u32 start_index;
    u32 index_count;

    material* material;
} surface;

typedef struct mesh {
    u32 id;
    char* name;

    // TODO: Stop using dynarray here
    surface* surfaces;  // Dynarray
    mesh_buffers buffers;
} mesh;

typedef struct render_object {
    char* mesh_name;
    char* material_name;

    u32 index_count;
    u32 first_index;
    VkBuffer index_buffer;

    material_pipeline pipeline;
    VkDescriptorSet material_set;

    m4s transform;
    VkDeviceAddress vertex_buffer_address;
} render_object;

typedef struct draw_context {
    render_object* opaque_surfaces;         // Dynarray
    render_object* transparent_surfaces;    // Dynarray
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

    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceVulkan11Properties properties_11;
    VkPhysicalDeviceVulkan12Properties properties_12;
    VkPhysicalDeviceVulkan13Properties properties_13;

    VkPhysicalDeviceFeatures features;
    VkPhysicalDeviceVulkan11Features features_11;
    VkPhysicalDeviceVulkan12Features features_12;
    VkPhysicalDeviceVulkan13Features features_13;

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