#pragma once

#include "defines.h"
#include "core/asserts.h"

#include "math/math_types.h"

#include <vulkan/vulkan.h>

/**
 * TODO: These should have renderer agnostic represenations for the engine:
 * struct image
 * struct buffer
 * struct GPU_scene_data
 * struct material_instance
 * struct geo_surface
 * struct mesh_asset
 * 
 * Not renderer specific:
 * struct renderable
 * struct node
 * struct mesh_node
 */

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

typedef struct gpu_mesh_buffers {
    buffer index_buffer;
    buffer vertex_buffer;
    VkDeviceAddress vertex_buffer_address;
} gpu_mesh_buffers;

typedef struct gpu_draw_push_constants {
    m4s world_matrix;
    VkDeviceAddress vertex_buffer;
} gpu_draw_push_constants;

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

// Could just: typedef struct material_instance GLTF_material here
typedef struct GLTF_material {
    char* name;
    material_instance data;
} GLTF_material;

typedef struct geo_surface {
    u32 start_index;
    u32 count;

    // Not responsible for freeing
    GLTF_material* material;
} geo_surface;

typedef struct mesh_asset {
    char* name;

    // Dynarray
    geo_surface* surfaces;
    gpu_mesh_buffers mesh_buffers;
} mesh_asset;

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
    char* name;
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

typedef struct loaded_gltf {
    // TODO: Have this extend renderable
    char* name;

    // The loaded_gltf struct currently stores the backing memory for 
    // it's meshes, images, materials, and nodes.
    mesh_asset* meshes;
    u32 mesh_count;
    image* images;
    u32 image_count;
    GLTF_material* materials;
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

    ds_allocator_growable descriptor_allocator;

    buffer material_data_buffer;

    renderer_state* render_state;
} loaded_gltf;

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