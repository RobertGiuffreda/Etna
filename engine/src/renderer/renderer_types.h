#pragma once

#include "defines.h"
#include "core/asserts.h"

#include <vulkan/vulkan.h>

#define FRAME_OVERLAP 2

#define VK_CHECK(expr) { ETASSERT((expr) == VK_SUCCESS); }

// TODO: GPU limits
typedef struct device {
    VkDevice handle;
    VkPhysicalDevice physical_device;

    // TODO: Better name which reflect that it is a queue family index
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

    // Window data
    VkSurfaceKHR surface;
    i32 width;
    i32 height;
    b8 resized;

    device device;

    // Swapchain data
    VkSwapchainKHR swapchain;
    VkFormat swapchain_image_format;
    VkImage* swapchain_images;
    VkImageView* swapchain_image_views;
    // Determines amount of frames: Length of perframe arrays
    u32 image_count;

    u32 current_frame;

    // TEMP: 
    i32 frame_number;
    // TEMP: END

    // NOTE: Per frame structures/data. Add to separate struct??
    VkSemaphore* swapchain_semaphores;
    VkSemaphore* render_semaphores;

    VkFence* render_fences;
    
    // Going to use the graphics queue for everything for now until I get a
    // better handle on things. 
    VkCommandPool* graphics_pools;
    VkCommandBuffer* main_graphics_command_buffers;
    // NOTE: Per frame END
} renderer_state;