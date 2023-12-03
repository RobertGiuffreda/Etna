#include "renderer_types.h"
#include "renderer.h"

#include "renderer/device.h"
#include "renderer/swapchain.h"

#include "window/renderer_window.h"

#include "containers/dynarray.h"

#include "core/etmemory.h"
#include "core/logger.h"
#include "core/etstring.h"

// TEMP: Until a math library is situated
#include <math.h>
// TEMP: END

static void create_frame_command_structures(renderer_state* state);
static void destroy_frame_command_structures(renderer_state* state);
static void create_frame_synchronization_structures(renderer_state* state);
static void destroy_frame_synchronization_structures(renderer_state* state);

VkBool32 vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData);

// NOTE: Exiting this function with return value false does not always free all allocated memory.
// This is fine for now as a return value of false is unrecoverable.
// But if there is a situation in the future where we run the function again to try again
// then we will be leaking memory 
b8 renderer_initialize(renderer_state** out_state, struct etwindow_state* window, const char* application_name) {
    renderer_state* state = etallocate(sizeof(renderer_state), MEMORY_TAG_RENDERER);
    etzero_memory(state, sizeof(renderer_state));
    
    state->allocator = 0;
    state->resized = false;
    state->swapchain = VK_NULL_HANDLE;
    state->current_frame = 0;

    // TEMP:
    state->frame_number = 0;
    // TEMP: END

    VkApplicationInfo app_cinfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = application_name,
        .applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
        .pEngineName = "Etna",
        .engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
        .apiVersion = VK_API_VERSION_1_3
    };

    i32 win_ext_count = 0;
    const char** window_extensions = window_get_required_extension_names(&win_ext_count);

    u32 required_extension_count = 0;
    const char** required_extensions = dynarray_create_data(win_ext_count, sizeof(char*), win_ext_count, window_extensions);
#ifdef _DEBUG
    const char* debug_extension = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    dynarray_push((void**)&required_extensions, &debug_extension);
#endif
    required_extension_count = dynarray_length(required_extensions);

    u32 required_layer_count = 0;
    const char** required_layers = 0;

#ifdef _DEBUG
    const char* layers[] = {
        "VK_LAYER_KHRONOS_validation",
    };
    required_layer_count = 1;
    required_layers = layers;
#endif

    // Validate extensions
    u32 supported_extension_count = 0;
    vkEnumerateInstanceExtensionProperties(0, &supported_extension_count, 0);
    VkExtensionProperties* supported_extensions =
        dynarray_create(supported_extension_count, sizeof(VkExtensionProperties));
    vkEnumerateInstanceExtensionProperties(0, &supported_extension_count, supported_extensions);
    for (u32 i = 0; i < required_extension_count; ++i) {
        b8 found = false;
        for (u32 j = 0; j < supported_extension_count; ++j) {
            b8 str_equal = strings_equal(required_extensions[i], supported_extensions[j].extensionName);
            if (str_equal) {
                found = true;
                ETINFO("Found required extension: '%s'.", required_extensions[i]);
                break;
            }
        }
        if (!found) {
            ETFATAL("Required extension missing: '%s'.", required_extensions[i]);
            return false;
        }
    }
    // Validate Layers
    u32 supported_layer_count = 0;
    vkEnumerateInstanceLayerProperties(&supported_layer_count, 0);
    VkLayerProperties* supported_layers = 
        dynarray_create(supported_layer_count, sizeof(VkLayerProperties));
    vkEnumerateInstanceLayerProperties(&supported_layer_count, supported_layers);

    for (u32 i = 0; i < required_layer_count; ++i) {
        b8 found = false;
        for (u32 j = 0; j < supported_layer_count; ++j) {
            b8 str_equal = strings_equal(required_layers[i], supported_layers[j].layerName);
            if (str_equal) {
                found = true;
                ETINFO("Found required layer: '%s'.", required_layers[i]);
                break;
            }
        }
        if (!found) {
            ETFATAL("Required layer missing: '%s'.", required_layers[i]);
            return false;
        }
    }

    // Clean up dynarrays
    dynarray_destroy(supported_extensions);
    dynarray_destroy(supported_layers);

    VkInstanceCreateInfo vkinstance_cinfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_cinfo,
        .enabledExtensionCount = required_extension_count,
        .ppEnabledExtensionNames = required_extensions,
        .enabledLayerCount = required_layer_count,
        .ppEnabledLayerNames = required_layers,
    };

    VK_CHECK(vkCreateInstance(&vkinstance_cinfo, state->allocator, &state->instance));
    ETINFO("Vulkan Instance Created");

    // Clean up required extension dynarray. Reuqired layers does not use dynarray
    dynarray_destroy(required_extensions);

#ifdef _DEBUG
    PFN_vkCreateDebugUtilsMessengerEXT pfnCreateDebugUtilsMessengerEXT =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(state->instance, "vkCreateDebugUtilsMessengerEXT");

    PFN_vkDestroyDebugUtilsMessengerEXT pfnDestroyDebugUtilsMessengerEXT =
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(state->instance, "vkDestroyDebugUtilsMessengerEXT");

    VkDebugUtilsMessengerCreateInfoEXT callback1 = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .pNext = NULL,
        .flags = 0,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
        .messageType= VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
        .pfnUserCallback = vk_debug_callback,
        .pUserData = NULL
    };
    VK_CHECK(pfnCreateDebugUtilsMessengerEXT(state->instance, &callback1, NULL, &state->debug_messenger));
    ETINFO("Vulkan debug messenger created.");
#endif

    if (!window_create_vulkan_surface(state, window)) {
        ETFATAL("Error creation vulkan surface");
        return false;
    }
    ETINFO("Vulkan surface created.");

    if(!device_create(state, &state->device)) {
        ETFATAL("Error creating vulkan device.");
        return false;
    }

    initialize_swapchain(state);

    create_frame_command_structures(state);
    ETINFO("Frame command structures created.");
    create_frame_synchronization_structures(state);
    ETINFO("Frame synchronization structures created.");

    *out_state = state;
    return true;
}

void renderer_shutdown(renderer_state* state) {
    vkDeviceWaitIdle(state->device.handle);
    // Destroy reverse order of creation

    destroy_frame_synchronization_structures(state);
    ETINFO("Frame synchronization structures destroyed.");

    destroy_frame_command_structures(state);
    ETINFO("Frame command structures destroyed.");

    shutdown_swapchain(state);
    ETINFO("Swapchain shutdown.");

    device_destory(state, state->device);
    ETINFO("Vulkan device destroyed");
    
    vkDestroySurfaceKHR(state->instance, state->surface, state->allocator);
    ETINFO("Vulkan surface destroyed");

#ifdef _DEBUG
    PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT =
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(state->instance, "vkDestroyDebugUtilsMessengerEXT");
    
    vkDestroyDebugUtilsMessengerEXT(state->instance, state->debug_messenger, state->allocator);
    ETDEBUG("Vulkan debugger destroyed");
#endif

    vkDestroyInstance(state->instance, state->allocator);
    ETINFO("Vulkan instance destroyed");

    etfree(state, sizeof(renderer_state), MEMORY_TAG_RENDERER);
}

b8 renderer_prepare_frame(renderer_state* state) {
    return true;
}

b8 renderer_draw_frame(renderer_state* state) {
    VkCommandBuffer frame_cmd = state->main_graphics_command_buffers[state->current_frame];

    // Wait for the current frame to end rendering by waiting on its render fence
    // Reset the render fence for reuse
    VK_CHECK(vkWaitForFences(
        state->device.handle,
        1,
        &state->render_fences[state->current_frame],
        VK_TRUE,
        1000000000));
    // TODO: Swapchain nonsense maybe
    VK_CHECK(vkResetFences(
        state->device.handle,
        1,
        &state->render_fences[state->current_frame]));
    
    u32 swapchain_index;
    VK_CHECK(vkAcquireNextImageKHR(
        state->device.handle,
        state->swapchain,
        0xFFFFFFFFFFFFFFFF,
        state->swapchain_semaphores[state->current_frame],
        VK_NULL_HANDLE,
        &swapchain_index));
    
    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = 0,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = 0};

    vkBeginCommandBuffer(frame_cmd, &begin_info);

    // Transition the swapchain image layout to writing suitable
    
    VkImageSubresourceRange acquire_subresource_range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1
    };

    VkImageMemoryBarrier2 acquire_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext = 0,

        .srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,

        .srcAccessMask = VK_ACCESS_2_NONE,
        .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,

        .oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,

        .image = state->swapchain_images[swapchain_index],
        .subresourceRange = acquire_subresource_range};

    // VkImageMemoryBarrier2 acquire_barrier = {
    //     .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
    //     .pNext = 0,

    //     .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
    //     .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,

    //     .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
    //     .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,

    //     .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    //     .newLayout = VK_IMAGE_LAYOUT_GENERAL,

    //     .image = state->swapchain_images[swapchain_index],
    //     .subresourceRange = acquire_subresource_range};
    
    VkDependencyInfo acquire_dependency = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = 0,
        .dependencyFlags = 0,
        .memoryBarrierCount = 0,
        .pBufferMemoryBarriers = 0,
        .bufferMemoryBarrierCount = 0,
        .pBufferMemoryBarriers = 0,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &acquire_barrier};

    vkCmdPipelineBarrier2(frame_cmd, &acquire_dependency);

    // Clear the screen
    VkClearColorValue clear_value;
    f32 flash = (f32)fabs(sin(state->frame_number / 120.0f));
    clear_value.float32[0] = 0.0f;
    clear_value.float32[1] = 0.0f;
    clear_value.float32[2] = flash;
    clear_value.float32[3] = 1.0f;

    VkImageSubresourceRange clear_range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1};
    
    vkCmdClearColorImage(frame_cmd,
        state->swapchain_images[swapchain_index],
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        &clear_value,
        1,
        &clear_range);

    // vkCmdClearColorImage(frame_cmd,
    //     state->swapchain_images[swapchain_index],
    //     VK_IMAGE_LAYOUT_GENERAL,
    //     &clear_value,
    //     1,
    //     &clear_range);

    VkImageSubresourceRange present_subresource_range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1};

    // Transition image back to presentable layout
    VkImageMemoryBarrier2 present_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext = 0,

        .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,

        .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_2_NONE,

        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,

        .image = state->swapchain_images[swapchain_index],
        .subresourceRange = present_subresource_range};

    // VkImageMemoryBarrier2 present_barrier = {
    //     .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
    //     .pNext = 0,

    //     .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
    //     .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,

    //     .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
    //     .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,

    //     .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
    //     .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,

    //     .image = state->swapchain_images[swapchain_index],
    //     .subresourceRange = present_subresource_range};
    
    VkDependencyInfo present_dependency = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = 0,
        .dependencyFlags = 0,
        .memoryBarrierCount = 0,
        .pBufferMemoryBarriers = 0,
        .bufferMemoryBarrierCount = 0,
        .pBufferMemoryBarriers = 0,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &present_barrier};

    vkCmdPipelineBarrier2(frame_cmd, &present_dependency);

    // End command buffer recording
    vkEndCommandBuffer(frame_cmd);

    // Begin command buffer submission
    VkCommandBufferSubmitInfo cmd_submit_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .pNext = 0,
        .commandBuffer = frame_cmd};

    VkSemaphoreSubmitInfo wait_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .pNext = 0,
        .semaphore = state->swapchain_semaphores[swapchain_index],
        .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT};

    VkSemaphoreSubmitInfo signal_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .pNext = 0,
        .semaphore = state->render_semaphores[state->current_frame],
        .stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT};
    
    VkSubmitInfo2 queue_submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .pNext = 0,
        .waitSemaphoreInfoCount = 1,
        .pWaitSemaphoreInfos = &wait_info,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &cmd_submit_info,
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos = &signal_info};
    
    // Submit commands
    VK_CHECK(vkQueueSubmit2(
        state->device.graphics_queue,
        1,
        &queue_submit_info,
        state->render_fences[state->current_frame]));

    // Present the image
    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = 0,
        .swapchainCount = 1,
        .pSwapchains = &state->swapchain,
        
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &state->render_semaphores[state->current_frame],
        
        .pImageIndices = &swapchain_index};
    
    VK_CHECK(vkQueuePresentKHR(state->device.present_queue, &present_info));

    state->frame_number++;

    // Move to the next frame
    state->current_frame = (state->current_frame + 1) % state->image_count;
    return true;
}

static void create_frame_command_structures(renderer_state* state) {
    state->graphics_pools = (VkCommandPool*)etallocate(
        sizeof(VkCommandPool) * state->image_count,
        MEMORY_TAG_RENDERER);
    state->main_graphics_command_buffers = (VkCommandBuffer*)etallocate(
        sizeof(VkCommandBuffer) * state->image_count,
        MEMORY_TAG_RENDERER);
    for (u32 i= 0; i < state->image_count; ++i) {
        VkCommandPoolCreateInfo graphics_pool_cinfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = 0,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = state->device.graphics_queue_index};
        VK_CHECK(vkCreateCommandPool(state->device.handle,
            &graphics_pool_cinfo,
            state->allocator,
            &state->graphics_pools[i]));
        VkCommandBufferAllocateInfo main_graphics_buffer_allocate_cinfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = 0,
            .commandPool = state->graphics_pools[i],
            .level = 0,
            .commandBufferCount = 1};
        VK_CHECK(vkAllocateCommandBuffers(
            state->device.handle,
            &main_graphics_buffer_allocate_cinfo,
            &state->main_graphics_command_buffers[i]));
    }
}

static void destroy_frame_command_structures(renderer_state* state) {
    // NOTE: Command buffers do not have to be freed to destroy the
    // associated command pool
    for (u32 i = 0; i < state->image_count; ++i) {
        vkDestroyCommandPool(
            state->device.handle,
            state->graphics_pools[i],
            state->allocator);
    }
    etfree(state->main_graphics_command_buffers,
           sizeof(VkCommandBuffer) * state->image_count,
           MEMORY_TAG_RENDERER);
    etfree(state->graphics_pools,
           sizeof(VkCommandPool) * state->image_count,
           MEMORY_TAG_RENDERER);
}

static void create_frame_synchronization_structures(renderer_state* state) {
    state->swapchain_semaphores = (VkSemaphore*)etallocate(
        sizeof(VkSemaphore) * state->image_count,
        MEMORY_TAG_RENDERER);
    state->render_semaphores = (VkSemaphore*)etallocate(
        sizeof(VkSemaphore) * state->image_count,
        MEMORY_TAG_RENDERER);
    state->render_fences = (VkFence*)etallocate(
        sizeof(VkFence) * state->image_count,
        MEMORY_TAG_RENDERER);

    VkSemaphoreCreateInfo semaphores_cinfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = 0,
        .flags = 0};
    VkFenceCreateInfo render_fence_cinfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = 0,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT};
    for (u32 i = 0; i < state->image_count; ++i) {
        VK_CHECK(vkCreateSemaphore(
            state->device.handle,
            &semaphores_cinfo,
            state->allocator,
            &state->swapchain_semaphores[i]));
        VK_CHECK(vkCreateSemaphore(
            state->device.handle, 
            &semaphores_cinfo, 
            state->allocator, 
            &state->render_semaphores[i]));
        VK_CHECK(vkCreateFence(state->device.handle,
            &render_fence_cinfo,
            state->allocator,
            &state->render_fences[i]));
    }
}

static void destroy_frame_synchronization_structures(renderer_state* state) {
    for (u32 i = 0; i < state->image_count; ++i) {
        vkDestroySemaphore(
            state->device.handle,
            state->swapchain_semaphores[i],
            state->allocator);
        vkDestroySemaphore(
            state->device.handle,
            state->render_semaphores[i],
            state->allocator);
        vkDestroyFence(
            state->device.handle,
            state->render_fences[i],
            state->allocator);
    }
    etfree(state->swapchain_semaphores,
          sizeof(VkSemaphore) * state->image_count,
          MEMORY_TAG_RENDERER);
    etfree(state->render_semaphores,
          sizeof(VkSemaphore) * state->image_count,
          MEMORY_TAG_RENDERER);
    etfree(state->render_fences,
           sizeof(VkFence) * state->image_count,
           MEMORY_TAG_RENDERER);
}

void renderer_on_resize(renderer_state* state, i32 width, i32 height) {
    state->width = width;
    state->height = height;
    state->resized = true;
}

VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) 
{
    switch(messageSeverity)
    {
        default:
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            ETERROR(pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            ETWARN(pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            ETINFO(pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            ETTRACE(pCallbackData->pMessage);
            break;
    }
    return VK_FALSE;
}

// // TODO: Move to another file. Image.h or something
// VkImageMemoryBarrier2 image_barrier(
//     VkCommandBuffer cmd,
//     VkImage img,
//     VkImageLayout old_layout,
//     VkImageLayout new_layout,
//     VkAccessFlags2 src_access_mask,
//     VkAccessFlags2 dst_access_mask,
//     VkPipelineStageFlags2 src_stage_mask,
//     VkPipelineStageFlags2 dst_stage_mask)
// {

// }