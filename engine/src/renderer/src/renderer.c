#include "renderer/rendererAPI.h"
#include "renderer.h"

#include "renderer/src/utilities/vkinit.h"
#include "renderer/src/utilities/vkutils.h"

#include "renderer/src/vk_types.h"
#include "renderer/src/device.h"
#include "renderer/src/swapchain.h"
#include "renderer/src/image.h"
#include "renderer/src/buffer.h"
#include "renderer/src/pipeline.h"
#include "renderer/src/shader.h"
#include "renderer/src/descriptor.h"
#include "renderer/src/GLTF_MR.h"
#include "renderer/src/renderables.h"

#include "window/renderer_window.h"

#include "containers/dynarray.h"

#include "memory/etmemory.h"
#include "core/logger.h"
#include "core/etstring.h"

#include "resources/importers/gltfimporter.h"

#include "scene/scene.h"

// TEMP: Until a math library is situated
#include <math.h>
// TEMP: END

// Initialize frame structure functions
static void create_frame_command_structures(renderer_state* state);
static void destroy_frame_command_structures(renderer_state* state);

static void create_frame_synchronization_structures(renderer_state* state);
static void destroy_frame_synchronization_structures(renderer_state* state);

static void initialize_descriptors(renderer_state* state);
static void shutdown_descriptors(renderer_state* state);

static void create_scene_data_buffers(renderer_state* state);
static void destroy_scene_data_buffers(renderer_state* state);

// TEMP: Until Compute effects framework. Meaning post processing effect
static b8 initialize_compute_effects(renderer_state* state);
static void shutdown_compute_effects(renderer_state* state);
// TEMP: END

// TEMP: Until material framework is stood up
static b8 initialize_default_material(renderer_state* state);
static void shutdown_default_material(renderer_state* state);
// TEMP: END

static b8 initialize_default_data(renderer_state* state);
static void shutdown_default_data(renderer_state* state);

static void draw_geometry(renderer_state* state, VkCommandBuffer cmd);

b8 rebuild_swapchain(renderer_state* state);

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
    state->swapchain_dirty = false;
    state->swapchain = VK_NULL_HANDLE;
    state->frame_index = 0;

    VkApplicationInfo app_cinfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = application_name,
        .applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
        .pEngineName = "Etna",
        .engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
        .apiVersion = VK_API_VERSION_1_3};

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
            b8 str_equal = strs_equal(required_extensions[i], supported_extensions[j].extensionName);
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
            b8 str_equal = strs_equal(required_layers[i], supported_layers[j].layerName);
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
        .ppEnabledLayerNames = required_layers,};
    VK_CHECK(vkCreateInstance(&vkinstance_cinfo, state->allocator, &state->instance));
    ETINFO("Vulkan Instance Created");

    // Clean up required extension dynarray. Reuqired layers does not use dynarray
    dynarray_destroy(required_extensions);

#ifdef _DEBUG
    PFN_vkCreateDebugUtilsMessengerEXT pfnCreateDebugUtilsMessengerEXT =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(state->instance, "vkCreateDebugUtilsMessengerEXT");

    PFN_vkDestroyDebugUtilsMessengerEXT pfnDestroyDebugUtilsMessengerEXT =
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(state->instance, "vkDestroyDebugUtilsMessengerEXT");

    // Function to set an objects debug name. For debugging
    state->vkSetDebugUtilsObjectNameEXT = 
        (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(state->instance, "vkSetDebugUtilsObjectNameEXT");

    VkDebugUtilsMessengerCreateInfoEXT callback1 = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .pNext = NULL,
        .flags = 0,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
        .messageType= VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
        .pfnUserCallback = vk_debug_callback,
        .pUserData = NULL};
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

    // TODO: state->window_extent should be set before the swapchain in case the 
    // swapchain current extent is 0xFFFFFFFF. Special value to say the app is in
    // control of the size 
    initialize_swapchain(state);

    // TEMP: HACK: TODO: Make render image resolution configurable from engine & not the window_extent
    VkExtent3D render_resolution = {
        .width = state->window_extent.width,
        .height = state->window_extent.height,
        .depth = 1};
    state->render_extent = render_resolution;

    // Rendering attachment images initialization
    
    // Color attachment
    VkImageUsageFlags draw_image_usages = {0};
    draw_image_usages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    draw_image_usages |= VK_IMAGE_USAGE_STORAGE_BIT;
    draw_image_usages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    image2D_create(state,
        state->render_extent,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        draw_image_usages,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &state->render_image);
    ETINFO("Render image created");

    // Depth attachment
    VkImageUsageFlags depth_image_usages = {0};
    depth_image_usages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    image2D_create(state, 
        state->render_extent,
        VK_FORMAT_D32_SFLOAT,
        depth_image_usages,
        VK_IMAGE_ASPECT_DEPTH_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &state->depth_image);
    ETINFO("Depth image created");

    // Per frame initializations
    create_frame_command_structures(state);
    ETINFO("Frame command structures created.");
    create_frame_synchronization_structures(state);
    ETINFO("Frame synchronization structures created.");
    create_scene_data_buffers(state);
    ETINFO("Frame scene data buffers created");

    initialize_descriptors(state);
    ETINFO("Descriptors Initialized.");

    if (!initialize_compute_effects(state)) {
        ETERROR("Could not initialize compute effects.");
        return false;
    }

    if (!initialize_default_material(state)) {
        ETERROR("Could not initialize default material.");
        return false;
    }

    if (!initialize_default_data(state)) {
        ETFATAL("Error intializing data.");
        return false;
    }

    *out_state = state;
    return true;
}

void renderer_shutdown(renderer_state* state) {
    vkDeviceWaitIdle(state->device.handle);
    // Destroy reverse order of creation

    shutdown_default_data(state);

    shutdown_default_material(state);

    shutdown_compute_effects(state);
    ETINFO("Compute effects shutdown.");

    destroy_scene_data_buffers(state);
    ETINFO("Scene data buffers destroyed.");

    shutdown_descriptors(state);
    ETINFO("Descriptors shutdown.");

    destroy_frame_synchronization_structures(state);
    ETINFO("Frame synchronization structures destroyed.");

    destroy_frame_command_structures(state);
    ETINFO("Frame command structures destroyed.");

    image_destroy(state, &state->depth_image);
    image_destroy(state, &state->render_image);
    ETINFO("Rendering attachments (color, depth) destroyed.");

    shutdown_swapchain(state);
    ETINFO("Swapchain shutdown.");

    device_destroy(state, &state->device);
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

b8 renderer_draw_frame(renderer_state* state) {
    // Wait for the current frame to end rendering by waiting on its render fence
    // Reset the render fence for reuse

    VkResult result = vkWaitForFences(
        state->device.handle,
        /* Fence count: */ 1,
        &state->render_fences[state->frame_index],
        VK_TRUE,
        1000000000);
    VK_CHECK(result);
    
    descriptor_set_allocator_clear_pools(&state->frame_allocators[state->frame_index], state);

    u32 swapchain_index;
    result = vkAcquireNextImageKHR(
        state->device.handle,
        state->swapchain,
        0xFFFFFFFFFFFFFFFF,
        state->swapchain_semaphores[state->frame_index],
        VK_NULL_HANDLE,
        &swapchain_index);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        rebuild_swapchain(state);
        return true;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        ETFATAL("Failed to acquire swapchain image.");
        return false;
    }

    VK_CHECK(vkResetFences(
        state->device.handle,
        1,
        &state->render_fences[state->frame_index]));

    VK_CHECK(vkResetCommandBuffer(state->main_graphics_command_buffers[state->frame_index], 0));

    VkCommandBuffer frame_cmd = state->main_graphics_command_buffers[state->frame_index];
    
    VkCommandBufferBeginInfo begin_info = init_command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(frame_cmd, &begin_info));

    image_barrier(frame_cmd, state->render_image.handle, state->render_image.aspects,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
        VK_ACCESS_2_NONE, VK_ACCESS_2_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_NONE, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

    // TEMP: Hardcode Gradient compute
    vkCmdBindPipeline(frame_cmd, VK_PIPELINE_BIND_POINT_COMPUTE, state->gradient_effect.pipeline);

    vkCmdBindDescriptorSets(frame_cmd, VK_PIPELINE_BIND_POINT_COMPUTE, state->gradient_effect.layout, 0, 1, &state->draw_image_descriptor_set, 0, 0);

    compute_push_constants pc = {
        .data1 = (v4s){1.0f, 0.0f, 0.0f, 1.0f},
        .data2 = (v4s){0.0f, 0.0f, 1.0f, 1.0f}};

    vkCmdPushConstants(frame_cmd, state->gradient_effect.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(compute_push_constants), &pc);

    vkCmdDispatch(frame_cmd, ceil(state->render_extent.width / 16.0f), ceil(state->render_extent.height / 16.0f), 1);
    // TEMP: END

    image_barrier(frame_cmd, state->render_image.handle, state->render_image.aspects,
        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_ACCESS_2_SHADER_WRITE_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

    image_barrier(frame_cmd, state->depth_image.handle, state->depth_image.aspects,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        VK_ACCESS_2_NONE, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT);

    // Draw the geometry
    draw_geometry(state, frame_cmd);

    // Image barrier to transition render image to transfer source optimal layout
    image_barrier(frame_cmd, state->render_image.handle, state->render_image.aspects,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT, VK_ACCESS_2_TRANSFER_READ_BIT,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT);

    // Image barrier to transition swapchain image to transfer destination optimal
    image_barrier(frame_cmd, state->swapchain_images[swapchain_index], VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_ACCESS_NONE, VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT);

    blit_image2D_to_image2D(
        frame_cmd,
        state->render_image.handle,
        state->swapchain_images[swapchain_index],
        state->render_extent,
        state->window_extent,
        VK_IMAGE_ASPECT_COLOR_BIT);

    // Image barrier to transition the swapchian image from transfer destination optimal to present khr
    image_barrier(frame_cmd, state->swapchain_images[swapchain_index], VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_ACCESS_2_TRANSFER_READ_BIT, VK_ACCESS_2_NONE,
        VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);

    // End command buffer recording
    VK_CHECK(vkEndCommandBuffer(frame_cmd));

    // Begin command buffer submission
    VkSemaphoreSubmitInfo wait_submit = init_semaphore_submit_info(
        state->swapchain_semaphores[state->frame_index],
        VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT);

    VkCommandBufferSubmitInfo cmd_submit = init_command_buffer_submit_info(frame_cmd);
    
    VkSemaphoreSubmitInfo signal_submit = init_semaphore_submit_info(
        state->render_semaphores[state->frame_index],
        VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT);

    VkSubmitInfo2 submit_info = init_submit_info2(
        1, &wait_submit,
        1, &cmd_submit,
        1, &signal_submit);
    
    // Submit commands
    result = vkQueueSubmit2(
        state->device.graphics_queue, 
        /* submitCount */ 1,
        &submit_info,
        state->render_fences[state->frame_index]);
    VK_CHECK(result);

    // Present the image
    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = 0,
        .swapchainCount = 1,
        .pSwapchains = &state->swapchain,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &state->render_semaphores[state->frame_index],
        .pImageIndices = &swapchain_index};
    result = vkQueuePresentKHR(state->device.present_queue, &present_info);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || state->swapchain_dirty) {
        rebuild_swapchain(state);
    } else if (result != VK_SUCCESS) {
        ETFATAL("Error presenting swapchain image");
        return false;
    }

    // Move to the next frame
    state->frame_index = (state->frame_index + 1) % state->image_count;
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
        VkCommandPoolCreateInfo gpool_info = init_command_pool_create_info(
            VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            state->device.graphics_qfi);
        VK_CHECK(vkCreateCommandPool(state->device.handle,
            &gpool_info,
            state->allocator,
            &state->graphics_pools[i]));
        
        // Allocate one primary
        VkCommandBufferAllocateInfo command_buffer_alloc_info = init_command_buffer_allocate_info(
            state->graphics_pools[i], VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);
        VK_CHECK(vkAllocateCommandBuffers(
            state->device.handle,
            &command_buffer_alloc_info,
            &state->main_graphics_command_buffers[i]));
    }
    // Immediate command pool & buffer
    VkCommandPoolCreateInfo imm_pool_info = init_command_pool_create_info(
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        state->device.graphics_qfi);
    VK_CHECK(vkCreateCommandPool(
        state->device.handle,
        &imm_pool_info,
        state->allocator,
        &state->imm_pool));

    VkCommandBufferAllocateInfo imm_buffer_alloc_info = init_command_buffer_allocate_info(
        state->imm_pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);
    VK_CHECK(vkAllocateCommandBuffers(
        state->device.handle,
        &imm_buffer_alloc_info,
        &state->imm_buffer));
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

    // Destroy immediate command pool
    vkDestroyCommandPool(state->device.handle, state->imm_pool, state->allocator);
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

    VkSemaphoreCreateInfo semaphore_info = init_semaphore_create_info(0);
    VkFenceCreateInfo fence_info = init_fence_create_info(
        VK_FENCE_CREATE_SIGNALED_BIT);
    
    for (u32 i = 0; i < state->image_count; ++i) {
        VK_CHECK(vkCreateSemaphore(
            state->device.handle,
            &semaphore_info,
            state->allocator,
            &state->swapchain_semaphores[i]));
        const char swap_sem_name[] = "Swapchain semaphore";
        SET_DEBUG_NAME(state, VK_OBJECT_TYPE_SEMAPHORE, state->swapchain_semaphores[i], swap_sem_name);
        VK_CHECK(vkCreateSemaphore(
            state->device.handle, 
            &semaphore_info, 
            state->allocator, 
            &state->render_semaphores[i]));
        const char rend_sem_name[] = "Render semaphore";
        SET_DEBUG_NAME(state, VK_OBJECT_TYPE_SEMAPHORE, state->render_semaphores[i], rend_sem_name);
        VK_CHECK(vkCreateFence(state->device.handle,
            &fence_info,
            state->allocator,
            &state->render_fences[i]));
    }

    // Create fence for synchronizing immediate command buffer submissions
    VK_CHECK(vkCreateFence(state->device.handle, &fence_info, state->allocator, &state->imm_fence));
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

    // Destroy fence for synchronizing the immediate command buffer
    vkDestroyFence(state->device.handle, state->imm_fence, state->allocator); 
}

static void initialize_descriptors(renderer_state* state) {
    // Update global allocator to growable version
    pool_size_ratio global_ratios[] = {
        { .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .ratio = 1},
        { .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .ratio = 1}};
    descriptor_set_allocator_initialize(&state->global_ds_allocator, 10, 2, global_ratios, state);

    // Descriptor set layout for accessing the render image from a compute shader
    dsl_builder dsl_builder = descriptor_set_layout_builder_create();
    descriptor_set_layout_builder_add_binding(&dsl_builder, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);
    state->draw_image_descriptor_set_layout = descriptor_set_layout_builder_build(&dsl_builder, state);
    descriptor_set_layout_builder_destroy(&dsl_builder);

    state->draw_image_descriptor_set = descriptor_set_allocator_allocate(&state->global_ds_allocator, state->draw_image_descriptor_set_layout, state);

    ds_writer writer = descriptor_set_writer_create_initialize();
    descriptor_set_writer_write_image(&writer, 0, state->render_image.view, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    descriptor_set_writer_update_set(&writer, state->draw_image_descriptor_set, state);
    descriptor_set_writer_shutdown(&writer);

    dsl_builder = descriptor_set_layout_builder_create();
    descriptor_set_layout_builder_add_binding(&dsl_builder, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    state->scene_data_descriptor_set_layout = descriptor_set_layout_builder_build(&dsl_builder, state);
    descriptor_set_layout_builder_destroy(&dsl_builder);

    pool_size_ratio frame_ratios[] = {
        { .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .ratio = 3},
        { .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .ratio = 3},
        { .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .ratio = 3},
        { .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .ratio = 3}};
    state->frame_allocators = etallocate(sizeof(ds_allocator) * state->image_count, MEMORY_TAG_RENDERER);
    for (u32 i = 0; i < state->image_count; ++i) {
        // Initialize growable allocator for the frames
        descriptor_set_allocator_initialize(&state->frame_allocators[i], 1000, 4, frame_ratios, state);
    }

    ETINFO("Descriptors Initialized");
}

// TODO: Should be reverse order of creation just for uniformity
static void shutdown_descriptors(renderer_state* state) {
    descriptor_set_allocator_shutdown(&state->global_ds_allocator, state);

    vkDestroyDescriptorSetLayout(state->device.handle, state->scene_data_descriptor_set_layout, state->allocator);

    vkDestroyDescriptorSetLayout(state->device.handle, state->draw_image_descriptor_set_layout, state->allocator);

    // Shutdown growable descriptor set allocators
    for (u32 i = 0; i < state->image_count; ++i) {
        descriptor_set_allocator_shutdown(&state->frame_allocators[i], state);
    }
    etfree(state->frame_allocators, sizeof(ds_allocator) * state->image_count, MEMORY_TAG_RENDERER);

    ETINFO("Descriptors shutdown");
}

static void create_scene_data_buffers(renderer_state* state) {
    state->scene_data = etallocate(sizeof(buffer) * state->image_count, MEMORY_TAG_RENDERER);
    for (u32 i = 0; i < state->image_count; ++i) {
        buffer_create(
            state,
            sizeof(scene_data),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &state->scene_data[i]
        );
    }
}

static void destroy_scene_data_buffers(renderer_state* state) {
    for (u32 i = 0; i < state->image_count; ++i) {
        buffer_destroy(state, &state->scene_data[i]);
    }
    etfree(state->scene_data, sizeof(buffer) * state->image_count, MEMORY_TAG_RENDERER);
}

static b8 initialize_compute_effects(renderer_state* state) {
    if (!load_shader(state, "build/assets/shaders/gradient.comp.spv", &state->gradient_shader)) {

    }

    VkPipelineLayoutCreateInfo compute_effect_pipeline_layout_info = init_pipeline_layout_create_info();
    compute_effect_pipeline_layout_info.setLayoutCount = 1;
    compute_effect_pipeline_layout_info.pSetLayouts = &state->draw_image_descriptor_set_layout;

    // Push constant structure for compute effect pipelines
    VkPushConstantRange push_constant = {0};
    push_constant.offset = 0;
    push_constant.size = sizeof(compute_push_constants);
    push_constant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    compute_effect_pipeline_layout_info.pushConstantRangeCount = 1;
    compute_effect_pipeline_layout_info.pPushConstantRanges = &push_constant;
    
    VK_CHECK(vkCreatePipelineLayout(state->device.handle, &compute_effect_pipeline_layout_info, state->allocator, &state->gradient_effect.layout));

    VkPipelineShaderStageCreateInfo compute_effect_pipeline_shader_stage_info = init_pipeline_shader_stage_create_info();
    compute_effect_pipeline_shader_stage_info.stage = state->gradient_shader.stage;
    compute_effect_pipeline_shader_stage_info.module = state->gradient_shader.module;
    compute_effect_pipeline_shader_stage_info.pName = state->gradient_shader.entry_point;

    VkComputePipelineCreateInfo compute_effect_info = init_compute_pipeline_create_info();
    compute_effect_info.layout = state->gradient_effect.layout;
    compute_effect_info.stage = compute_effect_pipeline_shader_stage_info;

    VK_CHECK(vkCreateComputePipelines(state->device.handle, VK_NULL_HANDLE, 1, &compute_effect_info, state->allocator, &state->gradient_effect.pipeline));
    return true;
}

static void shutdown_compute_effects(renderer_state* state) {
    vkDestroyPipeline(state->device.handle, state->gradient_effect.pipeline, state->allocator);
    vkDestroyPipelineLayout(state->device.handle, state->gradient_effect.layout, state->allocator);

    unload_shader(state, &state->gradient_shader);
}

static b8 initialize_default_material(renderer_state* state) {
    return GLTF_MR_build_blueprint(&state->metal_rough_material, state);
}

static void shutdown_default_material(renderer_state* state) {
    GLTF_MR_destroy_blueprint(&state->metal_rough_material, state);
}

static b8 initialize_default_data(renderer_state* state) {
    // NOTE: For some reason the u32 is being interpreted as 
    // 0xFFFFFFFF -- aabbggrr. Maybe gpu or computer specific
    u32 white = 0xFFFFFFFF;
    image2D_create_data(
        state,
        (void*)&white,
        (VkExtent3D){.width = 1, .height = 1, .depth = 1},
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &state->white_image);
    ETINFO("White default image created.");

    u32 grey = 0xFFAAAAAA;
    image2D_create_data(
        state,
        (void*)&grey,
        (VkExtent3D){.width = 1, .height = 1, .depth = 1},
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &state->grey_image);
    ETINFO("Grey default image created.");

    u32 black = 0xFF000000;
    image2D_create_data(
        state,
        (void*)&black,
        (VkExtent3D){.width = 1, .height = 1, .depth = 1},
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &state->black_image);
    ETINFO("Black default image created.");

    u32 magenta = 0xFFFF00FF;
    u32 pixels[16 * 16] = {0};
    for (u32 i = 0; i < 16; ++i) {
        for (u32 j = 0; j < 16; ++j) {
            pixels[j * 16 + i] = ((i % 2) ^ (j % 2)) ? magenta : black;
        }
    }
    image2D_create_data(
        state,
        (void*)&pixels[0],
        (VkExtent3D){.width = 16, .height = 16, .depth = 1},
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &state->error_image);
    ETINFO("Checkerboard default image created.");

    VkSamplerCreateInfo sampler_info = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};

    sampler_info.magFilter = VK_FILTER_NEAREST;
    sampler_info.minFilter = VK_FILTER_NEAREST;
    vkCreateSampler(state->device.handle, &sampler_info, state->allocator, &state->default_sampler_nearest);

    sampler_info.magFilter = VK_FILTER_LINEAR;
    sampler_info.minFilter = VK_FILTER_LINEAR;
    vkCreateSampler(state->device.handle, &sampler_info, state->allocator, &state->default_sampler_linear);

    struct material_resources mat_resources;
    mat_resources.color_image = state->white_image;
    mat_resources.color_sampler = state->default_sampler_linear;
    mat_resources.metal_rough_image = state->white_image;
    mat_resources.metal_rough_sampler = state->default_sampler_linear;

    // Create material_constants uniform buffer
    buffer_create(
        state,
        sizeof(struct material_constants),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &state->default_material_constants
    );

    struct material_constants scene_uniform_data = {
        .color_factors = (v4s){.raw = {1.f, 1.f, 1.f, 1.f}},
        .metal_rough_factors = (v4s){.raw = {1.f, .5f, 0.f, 0.f}},
    };
    struct material_constants* mapped_data;
    vkMapMemory(state->device.handle, state->default_material_constants.memory, 0, sizeof(struct material_constants), 0, (void**)&mapped_data);
    *mapped_data = scene_uniform_data;
    vkUnmapMemory(state->device.handle, state->default_material_constants.memory);

    mat_resources.data_buffer = state->default_material_constants.handle;
    mat_resources.data_buffer_offset = 0;

    state->default_material_instance = GLTF_MR_create_instance(&state->metal_rough_material, state, MATERIAL_PASS_MAIN_COLOR, &mat_resources, &state->global_ds_allocator);

    // Create a dynarray to store the draw context surface render objects
    state->main_draw_context.opaque_surfaces = dynarray_create(0, sizeof(render_object));
    state->main_draw_context.transparent_surfaces = dynarray_create(0, sizeof(render_object));
    return true;
}

static void shutdown_default_data(renderer_state* state) {
    dynarray_destroy(state->main_draw_context.transparent_surfaces);
    dynarray_destroy(state->main_draw_context.opaque_surfaces);

    buffer_destroy(state, &state->default_material_constants);

    vkDestroySampler(state->device.handle, state->default_sampler_linear, state->allocator);
    vkDestroySampler(state->device.handle, state->default_sampler_nearest, state->allocator);

    image_destroy(state, &state->error_image);
    image_destroy(state, &state->black_image);
    image_destroy(state, &state->grey_image);
    image_destroy(state, &state->white_image);
}

static void draw_geometry(renderer_state* state, VkCommandBuffer cmd) {
    VkRenderingAttachmentInfo color_attachment = init_color_attachment_info(
        state->render_image.view, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo depth_attachment = init_depth_attachment_info(
        state->depth_image.view, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    VkExtent2D render_extent = {.width = state->render_extent.width, .height = state->render_extent.height};
    VkRenderingInfo render_info = init_rendering_info(render_extent, &color_attachment, &depth_attachment);

    vkCmdBeginRendering(cmd, &render_info);

    VkViewport viewport = {0};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = render_extent.width;
    viewport.height = render_extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {0};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = render_extent.width;
    scissor.extent.height = render_extent.height;

    vkCmdSetScissor(cmd, 0, 1, &scissor);

    VkDescriptorSet scene_descriptor_set = descriptor_set_allocator_allocate(
        &state->frame_allocators[state->frame_index],
        state->scene_data_descriptor_set_layout,
        state);
    
    ds_writer writer = descriptor_set_writer_create_initialize();
    descriptor_set_writer_write_buffer(
        &writer,
        /* Binding: */ 0,
        state->scene_data[state->frame_index].handle,
        sizeof(scene_data),
        /* Offset: */ 0,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    descriptor_set_writer_update_set(&writer, scene_descriptor_set, state);
    descriptor_set_writer_shutdown(&writer);

    // TODO: record draw commands for transparent surfaces
    for (u32 i = 0; i < dynarray_length(state->main_draw_context.opaque_surfaces); ++i) {
        render_object* draw = &state->main_draw_context.opaque_surfaces[i];

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw->material->pipeline->pipeline);

        // Scene data/Global data descriptor set binding
        vkCmdBindDescriptorSets(cmd,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            draw->material->pipeline->layout, 
            /* First Set: */ 0,
            /* Descriptor Set Count: */ 1,
            &scene_descriptor_set,
            /* Dynamic offset count: */ 0,
            /* Dynamic offsets: */ 0);
        // Material descriptor set binding
        vkCmdBindDescriptorSets(cmd,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            draw->material->pipeline->layout,
            /* First Set: */ 1,
            /* Descriptor Set Count: */ 1,
            &draw->material->material_set,
            /* Dynamic offset count: */ 0,
            /* Dynamic offsets: */ 0);
        
        vkCmdBindIndexBuffer(cmd, draw->index_buffer, 0, VK_INDEX_TYPE_UINT32);

        gpu_draw_push_constants push_constants = {
            .vertex_buffer = draw->vertex_buffer_address,
            .world_matrix = draw->transform};
        vkCmdPushConstants(cmd, draw->material->pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(gpu_draw_push_constants), &push_constants);

        vkCmdDrawIndexed(cmd, draw->index_count, 1, draw->first_index, 0, 0);
    }

    vkCmdEndRendering(cmd);
}

void renderer_on_resize(renderer_state* state, i32 width, i32 height) {
    // state->window_extent.width = (u32)width;
    // state->window_extent.height = (u32)height;
    state->swapchain_dirty = true;
}

// Name is a bit confusing with recreate_swapchain around as well
b8 rebuild_swapchain(renderer_state* state) {
    // TODO: Use less heavy handed synchronization
    VK_CHECK(vkDeviceWaitIdle(state->device.handle));
    state->swapchain_dirty = false;
    if (!recreate_swapchain(state)) {
        ETERROR("Error recreating swapchain.");
        return false;
    }

    return true;
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
        // NOTE: The message is passed with ETXXXX(%s, pCallbackData->pMessage) instead of 
        // ETXXXX(pCallbackData->pMessage) as the message can have a % character followed by a zero,
        // which breaks a debug assertion in vsnprintf where it is passed as the format string even with
        // an empty variadic argument list.
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            ETERROR("%s", pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            ETWARN("%s", pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            ETINFO("%s", pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            ETTRACE("%s", pCallbackData->pMessage);
            break;
    }
    return VK_FALSE;
}

#ifdef _DEBUG
b8 renderer_set_debug_object_name(renderer_state* state, VkObjectType object_type, u64 object_handle, const char* object_name) {
    const VkDebugUtilsObjectNameInfoEXT name_info = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .pNext = 0,
        .objectType = object_type,
        .objectHandle = object_handle,
        .pObjectName = object_name};
    VK_CHECK(state->vkSetDebugUtilsObjectNameEXT(state->device.handle, &name_info));
    return true;
}
#endif