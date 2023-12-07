#pragma once

#include "defines.h"
#include "core/asserts.h"

#include <vulkan/vulkan.h>

#define FRAME_OVERLAP 2

#define VK_CHECK(expr) { ETASSERT((expr) == VK_SUCCESS); }

typedef struct image {
    VkImage handle;
    VkImageView view;
    VkDeviceMemory memory;

    VkImageType type;
    VkExtent3D extent;
    VkFormat format;
} image;

// TEMP: Very temporary until automation when I have a better
// handle on the needs of pipelines & shaders.
typedef struct compute_pipeline {
    VkPipeline handle;
    VkPipelineLayout layout;
    VkShaderModule shader;
    
    // TEMP: Very temporary until automation when I have a better
    // handle on the needs of pipelines & shaders.
    VkDescriptorSetLayout descriptor_layout;
    VkDescriptorSet descriptor_set;
} compute_pipeline;

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
} renderer_state;