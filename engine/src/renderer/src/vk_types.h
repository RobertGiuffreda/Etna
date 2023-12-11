#pragma once

#include "defines.h"
#include "core/asserts.h"

#include "math/math_types.h"

#include <vulkan/vulkan.h>

#define VK_CHECK(expr) { ETASSERT((expr) == VK_SUCCESS); }

typedef struct image {
    VkImage handle;
    VkImageView view;
    VkDeviceMemory memory;

    VkImageType type;
    VkExtent3D extent;
    VkFormat format;
} image;

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
    struct {
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
    }reflection;
} shader;

// TEMP: Very temporary until materials are properly implemented
typedef struct compute_pipeline {
    VkPipeline handle;
    VkPipelineLayout layout;
    VkShaderModule shader;
    
    // TEMP: Very temporary until materials are properly implemented
    VkDescriptorSetLayout descriptor_layout;
    VkDescriptorSet descriptor_set;
} compute_pipeline;

typedef struct graphics_pipeline_config {
    shader vertex_shader;
    shader fragment_shader;
} graphics_pipeline_config;

typedef struct graphics_pipeline {
    VkPipeline handle;
    VkPipelineLayout layout;

    // TEMP: Will be replaced by material shader
    VkDescriptorSetLayout descriptor_layout;
    VkDescriptorSet descriptor_set;
} graphics_pipeline;
// TEMP: END

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

    // TEMP: Descriptor set pool. Refactor to make descriptor set creation more automatic
    VkDescriptorPool descriptor_pool;
    // TEMP: END

    compute_pipeline test_comp_pipeline;

    graphics_pipeline_config test_graphics_config;
    graphics_pipeline test_graphics_pipeline;
} renderer_state;