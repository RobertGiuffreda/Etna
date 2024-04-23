#pragma once

#include "defines.h"
#include "core/asserts.h"

#include "math/math_types.h"
#include "renderer/renderer_types.h"
#include "resources/resource_types.h"

#define VK_ENABLE_BETA_EXTENSIONS
#include <vulkan/vulkan.h>

#define VK_CHECK(expr) { ETASSERT((expr) == VK_SUCCESS); }

// TODO: Remove, this is depricated
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
// TODO: END

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
    u64 size;
    VkBuffer handle;
    VkDeviceMemory memory;
} buffer;

// TEMP: Refactor this
typedef struct mesh_buffers {
    buffer index_buffer;
    buffer vertex_buffer;
} mesh_buffers;

typedef enum debug_view_type {
    DEBUG_VIEW_TYPE_OFF = 0,
    DEBUG_VIEW_TYPE_SHADOW,
    DEBUG_VIEW_TYPE_CURRENT_DEPTH,
    DEBUG_VIEW_TYPE_CLOSEST_DEPTH,
    DEBUG_VIEW_TYPE_METAL_ROUGH,
    DEBUG_VIEW_TYPE_NORMAL,
    DEBUG_VIEW_TYPE_MAX,
} debug_view_type;

typedef struct point_light {
	v4s color;
	v4s position;
} point_light;

typedef struct direction_light {
	v4s color;
	v4s direction;
} direction_light;

typedef struct scene_data {
    // Camera/View
    m4s view;
    m4s proj;
    m4s viewproj;
    v4s view_pos;

    // TEMP: Eventually define multiple lights
    v4s ambient_color;
    point_light light;

    m4s sun_viewproj;   // For shadow mapping
    direction_light sun;
    // TEMP: END
    
    // Kinda hacky spaghetti placement of this info
    float alpha_cutoff;
    u32 max_draw_count;
    u32 shadow_draw_id;
    u32 shadow_map_id;
    u32 debug_view;
} scene_data;

typedef struct draw_command {
    VkDrawIndexedIndirectCommand draw;
    u32 material_inst_id;
    u32 transform_id;
} draw_command;

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