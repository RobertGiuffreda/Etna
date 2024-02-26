#pragma once
#include "renderer/src/vk_types.h"

struct swapchain {
    VkSwapchainKHR swapchain;
    VkSurfaceKHR surface;
    VkFormat image_format;
    VkExtent3D image_extent;
    u32 image_count;
    VkImage* images;
    VkImageView* views;
    VkSemaphore* image_acquired; // Signaled
    VkSemaphore* image_present;  // Waited on

    u32 image_index;    // Index returned by acquire 
    u32 frame_index;    // Current frame index

    renderer_state* state;
};

b8 initialize_swapchain(renderer_state* state, swapchain* swapchain);

void shutdown_swapchain(renderer_state* state, swapchain* swapchain);

void recreate_swapchain(renderer_state* state, swapchain* swapchain);