#include "swapchain.h"

#include "data_structures/dynarray.h"
#include "core/logger.h"
#include "memory/etmemory.h"
#include "renderer/src/renderer.h"

b8 initialize_swapchain(renderer_state* state, swapchain* swapchain) {
    // Surface format detection & selection
    VkFormat image_format;
    VkColorSpaceKHR color_space;

    swapchain->image_index = 0;
    swapchain->frame_index = 0;
    
    u32 format_count = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
        state->device.gpu,
        swapchain->surface,
        &format_count,
        0));
    // TODO: Change from dynarray to regular allocation
    VkSurfaceFormatKHR* formats = dynarray_create(format_count, sizeof(VkSurfaceFormatKHR));
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
        state->device.gpu,
        swapchain->surface,
        &format_count,
        formats));

    b8 found = false;
    for (u32 i = 0; i < format_count; ++i) {
        // Preferred format
        if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM &&
            formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
        ) {    
            image_format = formats[i].format;
            color_space = formats[i].colorSpace;
            found = true;
            break;
        }
    }
    if (!found) {
        image_format = formats[0].format;
        color_space = formats[0].colorSpace;
    }
    dynarray_destroy(formats);

    VkSurfaceCapabilitiesKHR surface_capabilities;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        state->device.gpu,
        swapchain->surface,
        &surface_capabilities));

    VkExtent2D swapchain_extent;
    if (surface_capabilities.currentExtent.width == 0xFFFFFFFF) {
        swapchain_extent.width = swapchain->image_extent.width;
        swapchain_extent.height = swapchain->image_extent.height;
    } else {
        swapchain_extent = surface_capabilities.currentExtent;
    }

    // Clamp extent based on max & min values
    VkExtent2D min_extent = surface_capabilities.minImageExtent;
    VkExtent2D max_extent = surface_capabilities.maxImageExtent;
    // Minimum dimensions
    u32 clamp_width = (swapchain_extent.width < min_extent.width) ? min_extent.width : swapchain_extent.width;
    u32 clamp_height = (swapchain_extent.height < min_extent.height) ? min_extent.height : swapchain_extent.height;
    // Maximum dimensions
    swapchain_extent.width = (clamp_width > max_extent.width) ? max_extent.width : clamp_width;
    swapchain_extent.height = (clamp_height > max_extent.height) ? max_extent.height : clamp_height;

    // NOTE: This mode must be supported to adhere to vulkan spec
    u32 present_mode_count = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(state->device.gpu, swapchain->surface, &present_mode_count, 0));
    VkPresentModeKHR* present_modes = etallocate(sizeof(VkPresentModeKHR) * present_mode_count, MEMORY_TAG_SWAPCHAIN);
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(state->device.gpu, swapchain->surface, &present_mode_count, present_modes));

    // Get immediate mode if available for testing frame times
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
    for (u32 i = 0; i < present_mode_count; ++i) {
        if (present_modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) {
            present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        }
    }
    etfree(present_modes, sizeof(VkPresentModeKHR) * present_mode_count, MEMORY_TAG_SWAPCHAIN);

    u32 swapchain_image_count = surface_capabilities.minImageCount + 1;
    if (surface_capabilities.maxImageCount > 0 &&
        swapchain_image_count > surface_capabilities.maxImageCount
    ) {
        // Cannot go over max amount of images for swapchain
        swapchain_image_count = surface_capabilities.minImageCount;
    }

    VkSurfaceTransformFlagBitsKHR pre_transform;
    if (surface_capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
        pre_transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    } else {
        pre_transform = surface_capabilities.currentTransform;
    }

    // Find supported composite type
    VkCompositeAlphaFlagBitsKHR composite = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    VkCompositeAlphaFlagsKHR supported_composite_alpha = surface_capabilities.supportedCompositeAlpha;
	if (supported_composite_alpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) {
		composite = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	}
    else if (supported_composite_alpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR) {
		composite = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
	}
    else if (supported_composite_alpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR) {
		composite = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
	}
    else if (supported_composite_alpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR) {
		composite = VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
	}

    VkSwapchainCreateInfoKHR swapchain_cinfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = 0,
        .flags = 0,
        .surface = swapchain->surface,
        .minImageCount = swapchain_image_count,
        .imageFormat = image_format,
        .imageColorSpace = color_space,
        .imageExtent = swapchain_extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        // Image sharing mode & queue family indices below
        .preTransform = pre_transform,
        .compositeAlpha = composite,
        .presentMode = present_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE
    };

    u32 queue_family_indices[] = {
        (u32)state->device.graphics_qfi,
        (u32)state->device.present_qfi};
    if (state->device.graphics_qfi != state->device.present_qfi) {
        swapchain_cinfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchain_cinfo.queueFamilyIndexCount = 2;
        swapchain_cinfo.pQueueFamilyIndices = queue_family_indices;
    } else {
        swapchain_cinfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchain_cinfo.queueFamilyIndexCount = 0;
        swapchain_cinfo.pQueueFamilyIndices = 0;
    }

    VK_CHECK(vkCreateSwapchainKHR(
        state->device.handle,
        &swapchain_cinfo,
        state->allocator,
        &swapchain->swapchain));
    ETINFO("VkSwapchainKHR created.");

    swapchain->image_extent.width = swapchain_extent.width;
    swapchain->image_extent.height = swapchain_extent.height;
    swapchain->image_extent.depth = 1;
    swapchain->image_format = image_format;

    // Allocate memory for swapchain image handles and swapchain 
    // image view handles. Fetch swapchain images
    VK_CHECK(vkGetSwapchainImagesKHR(
        state->device.handle,
        swapchain->swapchain,
        &swapchain->image_count,
        0));
    swapchain->images = etallocate(
        sizeof(VkImage) * swapchain->image_count,
        MEMORY_TAG_SWAPCHAIN);
    swapchain->views = etallocate(
        sizeof(VkImageView) * swapchain->image_count,
        MEMORY_TAG_SWAPCHAIN);
    VK_CHECK(vkGetSwapchainImagesKHR(
        state->device.handle,
        swapchain->swapchain,
        &swapchain->image_count,
        swapchain->images));
    ETINFO("Swapchain images retrieved.");

    // Create swapchain image views
    for (u32 i = 0; i < swapchain->image_count; i++) {
        VkImageViewCreateInfo view_cinfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = swapchain->image_format,
            .image = swapchain->images[i],
            .subresourceRange.levelCount = 1,
            .subresourceRange.layerCount = 1,
            .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .components.r = VK_COMPONENT_SWIZZLE_R,
            .components.g = VK_COMPONENT_SWIZZLE_G,
            .components.b = VK_COMPONENT_SWIZZLE_B,
            .components.a = VK_COMPONENT_SWIZZLE_A};
        VK_CHECK(vkCreateImageView(
            state->device.handle,
            &view_cinfo,
            state->allocator,
            &swapchain->views[i]
        ));
        SET_DEBUG_NAME(state, VK_OBJECT_TYPE_IMAGE, swapchain->images[i], "Swapchain Image");
        SET_DEBUG_NAME(state, VK_OBJECT_TYPE_IMAGE_VIEW, swapchain->views[i], "Swapchain Image View");
    }
    ETINFO("Swapchain image views created");

    // Create image_acquire semaphores and image_present semaphores
    // TODO: If image count is different, recreate these in recreate swapchain
    swapchain->image_acquired = etallocate(
        sizeof(VkSemaphore) * swapchain->image_count,
        MEMORY_TAG_SWAPCHAIN);
    swapchain->image_present = etallocate(
        sizeof(VkSemaphore) * swapchain->image_count,
        MEMORY_TAG_SWAPCHAIN);
    VkSemaphoreCreateInfo semaphore_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = 0,
        .flags = 0};
    for (u32 i = 0; i < swapchain->image_count; ++i) {
        VK_CHECK(vkCreateSemaphore(
            state->device.handle,
            &semaphore_info,
            state->allocator,
            &swapchain->image_acquired[i]));
        const char acquire_sem[] = "Swapchain semaphore";
        SET_DEBUG_NAME(state, VK_OBJECT_TYPE_SEMAPHORE, swapchain->image_acquired[i], acquire_sem);
        VK_CHECK(vkCreateSemaphore(
            state->device.handle, 
            &semaphore_info, 
            state->allocator, 
            &swapchain->image_present[i]));
        const char present_sem[] = "Render semaphore";
        SET_DEBUG_NAME(state, VK_OBJECT_TYPE_SEMAPHORE, swapchain->image_present[i], present_sem);
    }
    ETINFO("Swapchain initialized");
    return true;
}

void shutdown_swapchain(renderer_state* state, swapchain* swapchain) {
    for (u32 i = 0; i < swapchain->image_count; ++i) {
        vkDestroyImageView(
            state->device.handle,
            swapchain->views[i],
            state->allocator
        );
        vkDestroySemaphore(state->device.handle, swapchain->image_acquired[i], state->allocator);
        vkDestroySemaphore(state->device.handle, swapchain->image_present[i], state->allocator);
    }
    vkDestroySwapchainKHR(state->device.handle, swapchain->swapchain, state->allocator);
    vkDestroySurfaceKHR(state->instance, swapchain->surface, state->allocator);
    etfree(swapchain->views,
        sizeof(VkImageView) * swapchain->image_count,
        MEMORY_TAG_SWAPCHAIN);
    etfree(swapchain->images,
        sizeof(VkImage) * swapchain->image_count,
        MEMORY_TAG_SWAPCHAIN);
    etfree(swapchain->image_acquired,
        sizeof(VkSemaphore) * swapchain->image_count,
        MEMORY_TAG_SWAPCHAIN);
    etfree(swapchain->image_present,
        sizeof(VkSemaphore) * swapchain->image_count,
        MEMORY_TAG_SWAPCHAIN
    );
    ETINFO("Swapchain shutdown.");
}

// TODO: Linear allocator for the swapchain images and views equal to the max number of images
void recreate_swapchain(renderer_state* state, swapchain* swapchain) {
    // Destroy the old swapchains image views
    for (u32 i = 0; i < swapchain->image_count; ++i) {
        vkDestroyImageView(
            state->device.handle,
            swapchain->views[i],
            state->allocator
        );
    }
    // Record old swapchain information
    VkSwapchainKHR old_swapchain = swapchain->swapchain;
    u32 old_image_count = swapchain->image_count;

    // Surface format detection & selection
    VkFormat image_format;
    VkColorSpaceKHR color_space;
    
    u32 format_count = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
        state->device.gpu,
        swapchain->surface,
        &format_count,
        NULL
    ));
    // TODO: Change from dynarray to regular allocation
    VkSurfaceFormatKHR* formats = dynarray_create(format_count, sizeof(VkSurfaceFormatKHR));
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
        state->device.gpu,
        swapchain->surface,
        &format_count,
        formats
    ));
    b8 found = false;
    for (u32 i = 0; i < format_count; ++i) {
        if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM &&
            formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
        ) {
            image_format = formats[i].format;
            color_space = formats[i].colorSpace;
            found = true;
            break;
        }
    }
    if (!found) {
        image_format = formats[0].format;
        color_space = formats[0].colorSpace;
    }
    dynarray_destroy(formats);

    VkSurfaceCapabilitiesKHR surface_capabilities;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        state->device.gpu,
        swapchain->surface,
        &surface_capabilities));

    VkExtent2D swapchain_extent;
    if (surface_capabilities.currentExtent.width == 0xFFFFFFFF) {
        swapchain_extent.width = swapchain->image_extent.width;
        swapchain_extent.height = swapchain->image_extent.height;
    } else {
        swapchain_extent = surface_capabilities.currentExtent;
    }

    // Clamp extent based on max & min values
    VkExtent2D min_extent = surface_capabilities.minImageExtent;
    VkExtent2D max_extent = surface_capabilities.maxImageExtent;
    // Minimum dimensions
    u32 clamp_width = (swapchain_extent.width < min_extent.width) ? min_extent.width : swapchain_extent.width;
    u32 clamp_height = (swapchain_extent.height < min_extent.height) ? min_extent.height : swapchain_extent.height;
    // Maximum dimensions
    swapchain_extent.width = (clamp_width > max_extent.width) ? max_extent.width : clamp_width;
    swapchain_extent.height = (clamp_height > max_extent.height) ? max_extent.height : clamp_height;

    // NOTE: This mode must be supported to adhere to vulkan spec
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;

    u32 swapchain_image_count = surface_capabilities.minImageCount + 1;
    if (surface_capabilities.maxImageCount > 0 &&
        swapchain_image_count > surface_capabilities.maxImageCount
    ) {
        // Cannot go over max amount of images for swapchain
        swapchain_image_count = surface_capabilities.maxImageCount;
    }

    VkSurfaceTransformFlagBitsKHR pre_transform;
    if (surface_capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
        pre_transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    } else {
        pre_transform = surface_capabilities.currentTransform;
    }

    // Find supported composite type
    VkCompositeAlphaFlagBitsKHR composite = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    VkCompositeAlphaFlagsKHR supported_composite_alpha = surface_capabilities.supportedCompositeAlpha;
	if (supported_composite_alpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) {
		composite = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	}
    else if (supported_composite_alpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR) {
		composite = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
	}
    else if (supported_composite_alpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR) {
		composite = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
	}
    else if (supported_composite_alpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR) {
		composite = VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
	}

    VkSwapchainCreateInfoKHR swapchain_cinfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = 0,
        .flags = 0,
        .surface = swapchain->surface,
        .minImageCount = swapchain_image_count,
        .imageFormat = image_format,
        .imageColorSpace = color_space,
        .imageExtent = swapchain_extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        // Image sharing mode & queue family indices below
        .preTransform = pre_transform,
        .compositeAlpha = composite,
        .presentMode = present_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = old_swapchain
    };

    u32 queue_family_indices[] = {
        (u32)state->device.graphics_qfi,
        (u32)state->device.present_qfi};
    if (state->device.graphics_qfi != state->device.present_qfi) {
        swapchain_cinfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchain_cinfo.queueFamilyIndexCount = 2;
        swapchain_cinfo.pQueueFamilyIndices = queue_family_indices;
    } else {
        swapchain_cinfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchain_cinfo.queueFamilyIndexCount = 0;
        swapchain_cinfo.pQueueFamilyIndices = 0;
    }

    VK_CHECK(vkCreateSwapchainKHR(
        state->device.handle,
        &swapchain_cinfo,
        state->allocator,
        &swapchain->swapchain));

    swapchain->image_extent.width = swapchain_extent.width;
    swapchain->image_extent.height = swapchain_extent.height;
    swapchain->image_extent.depth = 1;
    swapchain->image_format = image_format;

    // Destroy the old swapchain
    vkDestroySwapchainKHR(state->device.handle, old_swapchain, state->allocator);

    // Allocate memory for swapchain image handles and swapchain 
    // image view handles. Fetch swapchain images
    VK_CHECK(vkGetSwapchainImagesKHR(
        state->device.handle,
        swapchain->swapchain,
        &swapchain->image_count,
        0));
    // Reallocate memory if the image count is different
    if (old_image_count != swapchain->image_count) {
        // Reallocate memory for swapchain images
        etfree(swapchain->images,
            sizeof(VkImage) * old_image_count,
            MEMORY_TAG_SWAPCHAIN
        );
        swapchain->images = etallocate(
            sizeof(VkImage) * swapchain->image_count,
            MEMORY_TAG_SWAPCHAIN
        );
        // Reallocate memory for swapchain image views
        etfree(swapchain->views,
            sizeof(VkImageView) * old_image_count,
            MEMORY_TAG_SWAPCHAIN
        );
        swapchain->views = etallocate(
            sizeof(VkImageView) * swapchain->image_count,
            MEMORY_TAG_SWAPCHAIN
        );
    }
    VK_CHECK(vkGetSwapchainImagesKHR(
        state->device.handle,
        swapchain->swapchain,
        &swapchain->image_count,
        swapchain->images
    ));

    // Create swapchain image views
    for (u32 i = 0; i < swapchain->image_count; i++) {
        VkImageViewCreateInfo view_cinfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = swapchain->image_format,
            .image = swapchain->images[i],
            .subresourceRange.levelCount = 1,
            .subresourceRange.layerCount = 1,
            .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .components.r = VK_COMPONENT_SWIZZLE_R,
            .components.g = VK_COMPONENT_SWIZZLE_G,
            .components.b = VK_COMPONENT_SWIZZLE_B,
            .components.a = VK_COMPONENT_SWIZZLE_A};
        VK_CHECK(vkCreateImageView(
            state->device.handle,
            &view_cinfo,
            state->allocator,
            &swapchain->views[i]
        ));
    }
}