#pragma once

#include "defines.h"
#include "core/asserts.h"

#include "math/math_types.h"

#include <vulkan/vulkan.h>



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
/* NOTE: END */


typedef struct image {
    VkImage handle;
    VkImageView view;
    VkDeviceMemory memory;

    VkImageType type;
    VkExtent3D extent;
    VkFormat format;
} image;

// TODO: Buffer

// TODO: END

/** Information needed for each binding:
 * Requisite information for:
 * VkPipelineVertexInputStateCreateInfo
 * TODO: VkPipelineTessellationStateCreateInfo
 */
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
 * This section involving compute and graphics pipelines is a bit of a mess and
 * is temporary. This will be here until a material system is thought out and 
 * implemented.
 * 
 * Compute push constants are TEMP: and will be replaced with more robust system I think:
 * Compute shader post processing effects for now all share compute push constants to simplify things
 */

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

// TODO: Old and bad; about to remove
typedef struct compute_pipeline {
    VkPipeline handle;
    VkPipelineLayout layout;
    VkShaderModule shader;
    
    VkDescriptorSetLayout descriptor_layout;
    VkDescriptorSet descriptor_set;
} compute_pipeline;

typedef struct graphics_pipeline {
    VkPipeline handle;
    VkPipelineLayout layout;

    VkDescriptorSetLayout descriptor_layout;
    VkDescriptorSet descriptor_set;
} graphics_pipeline;
// TODO: END

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
#endif
    VkAllocationCallbacks* allocator;

    VkSurfaceKHR surface;
    i32 width;
    i32 height;
    b8 swapchain_dirty;

    device device;

    VkSwapchainKHR swapchain;
    VkFormat swapchain_image_format;

    // Determines amount of frames: Length of perframe arrays
    u32 image_count;
    VkImage* swapchain_images;
    VkImageView* swapchain_image_views;

    // Image to render to that get copied onto the swapchain image
    image render_image;

    // Index into per frame arrays: TODO: Rename to frame_index maybe??
    u32 current_frame; 
    
    // TEMP: THIS LINE
    i32 frame_number;

    // NOTE: Per frame structures/data. Add to separate struct??
    VkSemaphore* swapchain_semaphores;
    VkSemaphore* render_semaphores;

    VkFence* render_fences;
    
    // Going to use the graphics queue for everything for now until I get a
    // better handle on things. 
    VkCommandPool* graphics_pools;
    VkCommandBuffer* main_graphics_command_buffers;
    // NOTE: Per frame END

    ds_allocator global_ds_allocator;

    shader test_compute_shader;
    compute_effect test_compute_effect;

    // TEMP: Shared compute effect descriptor set layout
    VkDescriptorSetLayout test_compute_descriptor_set_layout;
    VkDescriptorSet test_compute_descriptor_set;
    // TEMP: END

    shader test_graphics_vertex;
    shader test_graphics_fragment;

    VkPipeline test_graphics_pipeline;
    VkPipelineLayout test_graphics_pipeline_layout;
    //TEMP: END
} renderer_state;