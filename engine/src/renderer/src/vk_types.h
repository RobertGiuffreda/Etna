#pragma once

#include "defines.h"
#include "core/asserts.h"

#include "math/math_types.h"
#include "resources/resource_types.h"

#include <vulkan/vulkan.h>

#define VK_CHECK(expr) { ETASSERT((expr) == VK_SUCCESS); }

typedef struct renderer_state renderer_state;

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

// TEMP: Until my vulkan memory management is implemented
// TODO: Store memory information for when implementing memory management
typedef struct image {
    u32 id;
    char* name;
    VkImage handle;
    VkImageView view;

    // TODO: Store memory requirements for mem management
    VkDeviceMemory memory;

    VkExtent3D extent;
    VkImageType type;
    VkFormat format;
    VkImageAspectFlags aspects;

    // For use in transfer system. 
    VkPipelineStageFlags2 stages; // NOTE: Unused
} image;

typedef struct buffer {
    char* name;
    VkBuffer handle;
    VkDeviceMemory memory;
    u64 size;

    // For use in transfer system.
    VkPipelineStageFlags2 stages; // NOTE: Unused
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

typedef struct gpu_scene_data {
    m4s view;
    m4s proj;
    m4s viewproj;
    v4s ambient_color;
    v4s sunlight_direction; // w for sun power
    v4s sunlight_color;
    v4s padding;            // 256 byte minimum on my GPU
} gpu_scene_data;

typedef struct material_pipeline {
    VkPipeline pipeline;
    VkPipelineLayout layout;
} material_pipeline;

typedef struct material_instance {
    material_pipeline* pipeline;
    VkDescriptorSet material_set;
    material_pass pass_type;
} material_instance;

// TODO: Rename material_constants, material_resources, GLTFMetallic_Roughness
// GLTFMetallic_Roughness is material format agnostic so the name does not make sense
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

typedef struct GLTFMetallic_Roughness {
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

    // Not responsible for freeing
    material* material;
} surface;

typedef struct mesh {
    u32 id;
    char* name;

    // Dynarray
    surface* surfaces;
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
    // Dynarray
    render_object* opaque_surfaces;
    render_object* transparent_surfaces;
} draw_context;
// TODO: END

// NOTE: Reflection info is unused at the moment 
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

// TODO: Reflection data used to create a material
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

    u32 input_count;
    struct {
        u32 location;
        VkFormat format;
    }*inputs;

    u32 output_count;
    struct {
        u32 location;
        VkFormat format;
    }*outputs;
} shader;

/** TEMP:TODO:
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
// TEMP:TODO: END

typedef struct device {
    VkDevice handle;
    VkPhysicalDevice gpu;
    VkPhysicalDeviceLimits gpu_limits;
    VkPhysicalDeviceMemoryProperties gpu_memory_props;

    // TODO: _queue_index --> _qfi
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