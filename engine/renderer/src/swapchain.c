#include "swapchain.h"

#include "data_structures/dynarray.h"

#include "memory/etmemory.h"
#include "core/logger.h"

#include "renderer/src/renderer.h"

b8 initialize_swapchain(renderer_state* state) {
    // Surface format detection & selection
    VkFormat image_format;
    VkColorSpaceKHR color_space;
    
    u32 format_count = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
        state->device.gpu,
        state->surface,
        &format_count,
        0));
    // TODO: Change from dynarray to regular allocation
    VkSurfaceFormatKHR* formats = dynarray_create(format_count, sizeof(VkSurfaceFormatKHR));
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
        state->device.gpu,
        state->surface,
        &format_count,
        formats));

    b8 found = false;
    for (u32 i = 0; i < format_count; ++i) {
        // Preferred format
        if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM &&
            formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {    
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
        state->surface,
        &surface_capabilities));

    VkExtent2D swapchain_extent;
    if (surface_capabilities.currentExtent.width == 0xFFFFFFFF) {
        swapchain_extent.width = state->window_extent.width;
        swapchain_extent.height = state->window_extent.height;
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
    if ((surface_capabilities.maxImageCount > 0) &&
        (swapchain_image_count > surface_capabilities.maxImageCount))
    {
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
        .surface = state->surface,
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
        &state->swapchain));
    ETINFO("Vulkan Swapchain intialized.");

    state->window_extent.width = swapchain_extent.width;
    state->window_extent.height = swapchain_extent.height;
    state->window_extent.depth = 1;
    state->swapchain_image_format = image_format;

    // Allocate memory for swapchain image handles and swapchain 
    // image view handles. Fetch swapchain images
    VK_CHECK(vkGetSwapchainImagesKHR(
        state->device.handle,
        state->swapchain,
        &state->image_count,
        0));
    state->swapchain_images = (VkImage*)etallocate(
        sizeof(VkImage) * state->image_count,
        MEMORY_TAG_RENDERER);
    state->swapchain_image_views = (VkImageView*)etallocate(
        sizeof(VkImageView) * state->image_count,
        MEMORY_TAG_RENDERER);
    VK_CHECK(vkGetSwapchainImagesKHR(
        state->device.handle,
        state->swapchain,
        &state->image_count,
        state->swapchain_images));
    ETINFO("Swapchain images retrieved.");

    // Create swapchain image views
    for (u32 i = 0; i < state->image_count; i++) {
        VkImageViewCreateInfo view_cinfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = state->swapchain_image_format,
            .image = state->swapchain_images[i],
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
            &state->swapchain_image_views[i]));
    }
    ETINFO("Swapchain image views created");
    return true;
}

void shutdown_swapchain(renderer_state* state) {
    for (u32 i = 0; i < state->image_count; ++i) {
        vkDestroyImageView(
            state->device.handle,
            state->swapchain_image_views[i],
            state->allocator);
    }
    vkDestroySwapchainKHR(state->device.handle, state->swapchain, state->allocator);

    etfree(state->swapchain_image_views,
        sizeof(VkImageView) * state->image_count,
        MEMORY_TAG_RENDERER);
    etfree(state->swapchain_images,
        sizeof(VkImage) * state->image_count,
        MEMORY_TAG_RENDERER);
}

// TODO: Linear allocator for the swapchain images and views equal to the max number of images
b8 recreate_swapchain(renderer_state* state) {
    // Destroy the old swapchains image views
    for (u32 i = 0; i < state->image_count; ++i) {
        vkDestroyImageView(
            state->device.handle,
            state->swapchain_image_views[i],
            state->allocator);
    }
    // Record old swapchain information
    VkSwapchainKHR old_swapchain = state->swapchain;
    u32 old_image_count = state->image_count;

    // Surface format detection & selection
    VkFormat image_format;
    VkColorSpaceKHR color_space;
    
    u32 format_count = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
        state->device.gpu,
        state->surface,
        &format_count,
        0));
    // TODO: Change from dynarray to regular allocation
    VkSurfaceFormatKHR* formats = dynarray_create(format_count, sizeof(VkSurfaceFormatKHR));
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
        state->device.gpu,
        state->surface,
        &format_count,
        formats));

    b8 found = false;
    for (u32 i = 0; i < format_count; ++i) {
        // Preferred format
        if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM &&
            formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {    
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
        state->surface,
        &surface_capabilities));

    VkExtent2D swapchain_extent;
    if (surface_capabilities.currentExtent.width == 0xFFFFFFFF) {
        swapchain_extent.width = state->window_extent.width;
        swapchain_extent.height = state->window_extent.height;
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
    if ((surface_capabilities.maxImageCount > 0) &&
        (swapchain_image_count > surface_capabilities.maxImageCount))
    {
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
        .surface = state->surface,
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
        &state->swapchain));

    state->window_extent.width = swapchain_extent.width;
    state->window_extent.height = swapchain_extent.height;
    state->window_extent.depth = 1;
    state->swapchain_image_format = image_format;

    // Destroy the old swapchain
    vkDestroySwapchainKHR(state->device.handle, old_swapchain, state->allocator);

    // Allocate memory for swapchain image handles and swapchain 
    // image view handles. Fetch swapchain images
    VK_CHECK(vkGetSwapchainImagesKHR(
        state->device.handle,
        state->swapchain,
        &state->image_count,
        0));
    // Reallocate memory if the image count is different
    if (old_image_count != state->image_count) {
        // Reallocate memory for swapchain images
        etfree(state->swapchain_images,
            sizeof(VkImage) * old_image_count,
            MEMORY_TAG_RENDERER
        );
        state->swapchain_images = (VkImage*)etallocate(
            sizeof(VkImage) * state->image_count,
            MEMORY_TAG_RENDERER
        );
        // Reallocate memory for swapchain image views
        etfree(state->swapchain_image_views,
            sizeof(VkImageView) * old_image_count,
            MEMORY_TAG_RENDERER
        );
        state->swapchain_image_views = (VkImageView*)etallocate(
            sizeof(VkImageView) * state->image_count,
            MEMORY_TAG_RENDERER
        );
    }
    VK_CHECK(vkGetSwapchainImagesKHR(
        state->device.handle,
        state->swapchain,
        &state->image_count,
        state->swapchain_images));

    // Create swapchain image views
    for (u32 i = 0; i < state->image_count; i++) {
        VkImageViewCreateInfo view_cinfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = state->swapchain_image_format,
            .image = state->swapchain_images[i],
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
            &state->swapchain_image_views[i]));
    }
    return true;
}