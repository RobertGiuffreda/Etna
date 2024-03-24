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

// TODO: Remove id as it gives wrong impression & implement suballocation
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
    VkSampleCountFlagBits ms;
} image;

typedef struct buffer {
    char* name;
    u64 size;
    VkBuffer handle;
    VkDeviceMemory memory;
} buffer;
// TODO: END

// TEMP: Refactor this
typedef struct mesh_buffers {
    buffer index_buffer;
    buffer vertex_buffer;
} mesh_buffers;

typedef struct scene_data {
    m4s view;
    m4s proj;
    m4s viewproj;
    v4s view_pos;
    v4s ambient_color;
    v4s light_color;
    v4s light_position;
    v4s sun_color;
    v4s sun_direction;
    u32 max_draw_count;
} scene_data;

typedef struct draw_command {
    VkDrawIndexedIndirectCommand draw;
    u32 material_inst_id;
    u32 transform_id;
} draw_command;

// TEMP:TODO: Design abstraction to interface with compute shaders in GPU driven manor
typedef struct compute_push_constants {
    v4s data1;
    v4s data2;
    v4s data3;
    v4s data4;
} compute_push_constants;

typedef struct compute_effect {
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