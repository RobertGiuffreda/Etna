#include "renderer/src/vk_types.h"
#include "renderer.h"

#include "renderer/src/utilities/vkinit.h"

#include "renderer/src/device.h"
#include "renderer/src/swapchain.h"
#include "renderer/src/image.h"
#include "renderer/src/buffer.h"
#include "renderer/src/pipeline.h"
#include "renderer/src/shader.h"
#include "renderer/src/descriptor.h"

#include "window/renderer_window.h"

#include "containers/dynarray.h"

#include "core/etmemory.h"
#include "core/logger.h"
#include "core/etstring.h"

#include "loaders/gltfloader.h"

// TEMP: Until a math library is situated
#include <math.h>
// TEMP: END

static void create_frame_command_structures(renderer_state* state);
static void destroy_frame_command_structures(renderer_state* state);

static void create_frame_synchronization_structures(renderer_state* state);
static void destroy_frame_synchronization_structures(renderer_state* state);

static void initialize_descriptors(renderer_state* state);
static void shutdown_descriptors(renderer_state* state);

// TEMP: Until material framework is stood up
static void initialize_triangle_pipeline(renderer_state* state);
static void shutdown_triangle_pipeline(renderer_state* state);

static void initialize_mesh_pipeline(renderer_state* state);
static void shutdown_mesh_pipeline(renderer_state* state);

static b8 intialize_default_data(renderer_state* state);
static void shutdown_default_data(renderer_state* state);

static void draw_geometry(renderer_state* state, VkCommandBuffer cmd);
// TEMP: END

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
    state->current_frame = 0;

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

    initialize_swapchain(state);

    // Rendering attachment images initialization

    VkImageUsageFlags draw_image_usages = {0};
    draw_image_usages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    draw_image_usages |= VK_IMAGE_USAGE_STORAGE_BIT;
    draw_image_usages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    // Color attachment
    VkExtent3D render_image_extent = {
        .width = state->width,
        .height = state->height,
        .depth = 1};
    image2D_create(state,
        render_image_extent,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        draw_image_usages,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &state->render_image);
    ETINFO("Render image created");

    VkImageUsageFlags depth_image_usages = {0};
    depth_image_usages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    VkExtent3D depth_image_extent = {
        .width = state->width,
        .height = state->height,
        .depth = 1};
    image2D_create(state, 
        depth_image_extent,
        VK_FORMAT_D32_SFLOAT,
        depth_image_usages,
        VK_IMAGE_ASPECT_DEPTH_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &state->depth_image);
    ETINFO("Depth image created");

    create_frame_command_structures(state);
    ETINFO("Frame command structures created.");
    create_frame_synchronization_structures(state);
    ETINFO("Frame synchronization structures created.");

    // This function initializes:
    // state->gradient_descriptor_set_layout
    // state->gradient_descriptor_set
    // state->global_ds_allocator
    initialize_descriptors(state);
    ETINFO("Descriptors Initialized.");

    // TEMP: Until post processing effect structure/system is created/becomes more robust. 
    load_shader(state, "build/assets/shaders/gradient.comp.spv", &state->gradient_shader);

    VkPipelineLayoutCreateInfo compute_effect_pipeline_layout_info = init_pipeline_layout_create_info();
    compute_effect_pipeline_layout_info.setLayoutCount = 1;
    compute_effect_pipeline_layout_info.pSetLayouts = &state->gradient_descriptor_set_layout;

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
    ETINFO("Gradient compute effect created.");
    // TEMP: END

    initialize_triangle_pipeline(state);

    initialize_mesh_pipeline(state);

    if (!intialize_default_data(state)) {
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

    shutdown_mesh_pipeline(state);

    shutdown_triangle_pipeline(state);

    // TEMP: Until post processing effect structure/system is created/becomes more robust. 
    vkDestroyPipeline(state->device.handle, state->gradient_effect.pipeline, state->allocator);
    vkDestroyPipelineLayout(state->device.handle, state->gradient_effect.layout, state->allocator);
    ETINFO("Gradient compute effect pipeline & Gradient compute effect pipeline layout destroyed.");

    unload_shader(state, &state->gradient_shader);
    ETINFO("Gradient compute shader unloaded.");
    // TEMP: Until post processing effect structure/system is created/becomes more robust. 

    shutdown_descriptors(state);
    ETINFO("Descriptors shutdown.");

    destroy_frame_synchronization_structures(state);
    ETINFO("Frame synchronization structures destroyed.");

    destroy_frame_command_structures(state);
    ETINFO("Frame command structures destroyed.");

    image2D_destroy(state, &state->depth_image);
    image2D_destroy(state, &state->render_image);
    ETINFO("Rendering attachments (color, depth) destroyed.");

    shutdown_swapchain(state);
    ETINFO("Swapchain shutdown.");

    device_destory(state, &state->device);
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
    // Wait for the current frame to end rendering by waiting on its render fence
    // Reset the render fence for reuse
    VK_CHECK(vkWaitForFences(
        state->device.handle,
        1,
        &state->render_fences[state->current_frame],
        VK_TRUE,
        1000000000));
    
    u32 swapchain_index;
    VK_CHECK(vkAcquireNextImageKHR(
        state->device.handle,
        state->swapchain,
        0xFFFFFFFFFFFFFFFF,
        state->swapchain_semaphores[state->current_frame],
        VK_NULL_HANDLE,
        &swapchain_index));

    VK_CHECK(vkResetFences(
        state->device.handle,
        1,
        &state->render_fences[state->current_frame]));

    VK_CHECK(vkResetCommandBuffer(state->main_graphics_command_buffers[state->current_frame], 0));

    VkCommandBuffer frame_cmd = state->main_graphics_command_buffers[state->current_frame];
    
    VkCommandBufferBeginInfo begin_info = init_command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(frame_cmd, &begin_info));

    // Image barrier to transition render_image from undefined to general layout for clear value command
    image_barrier(frame_cmd, state->render_image.handle, state->render_image.aspects,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
        VK_ACCESS_2_TRANSFER_READ_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT , VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

    // TEMP: Hardcode Gradient compute
    vkCmdBindPipeline(frame_cmd, VK_PIPELINE_BIND_POINT_COMPUTE, state->gradient_effect.pipeline);

    vkCmdBindDescriptorSets(frame_cmd, VK_PIPELINE_BIND_POINT_COMPUTE, state->gradient_effect.layout, 0, 1, &state->gradient_descriptor_set, 0, 0);

    compute_push_constants pc = {
        .data1 = (v4s){1.0f, 0.0f, 0.0f, 1.0f},
        .data2 = (v4s){0.0f, 0.0f, 1.0f, 1.0f}};

    vkCmdPushConstants(frame_cmd, state->gradient_effect.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(compute_push_constants), &pc);

    vkCmdDispatch(frame_cmd, ceil(state->width / 16.0f), ceil(state->height / 16.0f), 1);
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

    // Blit command to copy from render image to swapchain
    VkExtent3D blit_extent = {
        .width = state->width,
        .height = state->height,
        .depth = 1};
    blit_image2D_to_image2D(
        frame_cmd,
        state->render_image.handle,
        state->swapchain_images[swapchain_index],
        blit_extent,
        VK_IMAGE_ASPECT_COLOR_BIT);

    // Image barrier to transition the swapchian image from transfer destination optimal to present khr
    image_barrier(frame_cmd, state->swapchain_images[swapchain_index], VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_ACCESS_2_TRANSFER_READ_BIT, VK_ACCESS_2_NONE,
        VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);

    // End command buffer recording
    VK_CHECK(vkEndCommandBuffer(frame_cmd));

    // Begin command buffer submission
    VkCommandBufferSubmitInfo cmd_submit = init_command_buffer_submit_info(frame_cmd);
    VkSemaphoreSubmitInfo wait_submit = init_semaphore_submit_info(
        state->swapchain_semaphores[swapchain_index],
        VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT);
    VkSemaphoreSubmitInfo signal_submit = init_semaphore_submit_info(
        state->render_semaphores[state->current_frame],
        VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT);
    VkSubmitInfo2 submit_info = init_submit_info2(
        1, &wait_submit,
        1, &cmd_submit,
        1, &signal_submit);
    
    // Submit commands
    VK_CHECK(vkQueueSubmit2(state->device.graphics_queue, 1,
        &submit_info, state->render_fences[state->current_frame]));

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
        VkCommandPoolCreateInfo gpool_info = init_command_pool_create_info(
            VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            state->device.graphics_queue_index);
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
        state->device.graphics_queue_index);
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
        VK_CHECK(vkCreateSemaphore(
            state->device.handle, 
            &semaphore_info, 
            state->allocator, 
            &state->render_semaphores[i]));
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

    // Destory fence for synchronizing the immediate command buffer
    vkDestroyFence(state->device.handle, state->imm_fence, state->allocator); 
}

static void initialize_descriptors(renderer_state* state) {
    // Define pool sizes for descriptor allocator
    VkDescriptorPoolSize pool_sizes[] = {
        { .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 1},
    };
    VkDescriptorPoolSize* dynarray_pools = dynarray_create_data(1, sizeof(VkDescriptorPoolSize), 1, pool_sizes);
    descriptor_set_allocator_initialize(&state->global_ds_allocator, 10, dynarray_pools, state);
    dynarray_destroy(dynarray_pools);
    ETINFO("Descriptor set allocator initialized.");

    dsl_builder dsl_builder = descriptor_set_layout_builder_create();
    descriptor_set_layout_builder_add_binding(&dsl_builder, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);
    state->gradient_descriptor_set_layout = descriptor_set_layout_builder_build(&dsl_builder, state);
    ETINFO("Gradient Compute Descriptor Set Layout created.");

    state->gradient_descriptor_set = descriptor_set_allocator_allocate(&state->global_ds_allocator, state->gradient_descriptor_set_layout, state);
    descriptor_set_layout_builder_destroy(&dsl_builder);
    ETINFO("Gradient Compute Descriptor Set allocated");

    ds_writer writer = descriptor_set_writer_create_initialize();
    descriptor_set_writer_write_image(&writer, 0, state->render_image.view, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    descriptor_set_writer_update_set(&writer, state->gradient_descriptor_set, state);
    descriptor_set_writer_shutdown(&writer);
    ETINFO("Gradient Compute Descriptor Set updated.");
}

static void shutdown_descriptors(renderer_state* state) {
    descriptor_set_allocator_shutdown(&state->global_ds_allocator, state);
    ETINFO("Descriptor Set Allocator destroyed.");

    vkDestroyDescriptorSetLayout(state->device.handle, state->gradient_descriptor_set_layout, state->allocator);
    ETINFO("Gradient compute descriptor set layout destroyed.");
}

static void initialize_triangle_pipeline(renderer_state* state) {
    load_shader(state, "build/assets/shaders/triangle.vert.spv", &state->triangle_vertex);
    load_shader(state, "build/assets/shaders/triangle.frag.spv", &state->triangle_fragment);
    ETINFO("Triangle pipeline shaders loaded.");

    VkPipelineLayoutCreateInfo layout_info = init_pipeline_layout_create_info();
    VK_CHECK(vkCreatePipelineLayout(state->device.handle, &layout_info, state->allocator, &state->triangle_pipeline_layout));

    pipeline_builder builder = pipeline_builder_create();
    builder.layout = state->triangle_pipeline_layout;
    pipeline_builder_set_shaders(&builder,
        state->triangle_vertex,
        state->triangle_fragment);
    pipeline_builder_set_input_topology(&builder, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipeline_builder_set_polygon_mode(&builder, VK_POLYGON_MODE_FILL);
    pipeline_builder_set_cull_mode(&builder, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipeline_builder_set_multisampling_none(&builder);
    pipeline_builder_disable_blending(&builder);
    pipeline_builder_disable_depthtest(&builder);
    pipeline_builder_set_color_attachment_format(&builder, state->render_image.format);
    pipeline_builder_set_depth_attachment_format(&builder, state->depth_image.format);
    state->triangle_pipeline = pipeline_builder_build(&builder, state);

    pipeline_builder_destroy(&builder);

    ETINFO("Triangle pipeline & pipeline layout created");
}

static void shutdown_triangle_pipeline(renderer_state* state) {
    vkDestroyPipeline(state->device.handle, state->triangle_pipeline, state->allocator);
    vkDestroyPipelineLayout(state->device.handle, state->triangle_pipeline_layout, state->allocator);
    ETINFO("Triangle pipeline & Triangle pipeline layout destroyed.");

    unload_shader(state, &state->triangle_vertex);
    unload_shader(state, &state->triangle_fragment);
    ETINFO("Triangle pipeline shaders unloaded.");
}

static void initialize_mesh_pipeline(renderer_state* state) {
    load_shader(state, "build/assets/shaders/mesh.vert.spv", &state->mesh_vertex);
    load_shader(state, "build/assets/shaders/triangle.frag.spv", &state->mesh_fragment);
    ETINFO("Mesh pipeline shaders loaded.");

    VkPushConstantRange buffer_range = {
        .offset = 0,
        .size = sizeof(gpu_draw_push_constants),
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT};
    
    VkPipelineLayoutCreateInfo pipeline_layout_info = init_pipeline_layout_create_info();
    pipeline_layout_info.pushConstantRangeCount = 1;
    pipeline_layout_info.pPushConstantRanges = &buffer_range;

    VK_CHECK(vkCreatePipelineLayout(
        state->device.handle,
        &pipeline_layout_info,
        state->allocator,
        &state->mesh_pipeline_layout));
    pipeline_builder builder = pipeline_builder_create();
    builder.layout = state->mesh_pipeline_layout;
    pipeline_builder_set_shaders(&builder, state->mesh_vertex, state->mesh_fragment);
    pipeline_builder_set_input_topology(&builder, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipeline_builder_set_polygon_mode(&builder, VK_POLYGON_MODE_FILL);
    pipeline_builder_set_cull_mode(&builder, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipeline_builder_set_multisampling_none(&builder);
    // pipeline_builder_disable_blending(&builder);
    pipeline_builder_enable_blending_additive(&builder);
    // pipeline_builder_enable_blending_alphablend(&builder);
    pipeline_builder_enable_depthtest(&builder, true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    pipeline_builder_set_color_attachment_format(&builder, state->render_image.format);
    pipeline_builder_set_depth_attachment_format(&builder, state->depth_image.format);
    state->mesh_pipeline = pipeline_builder_build(&builder, state);
    
    pipeline_builder_destroy(&builder);

    ETINFO("Mesh pipeline & pipeline layout created");
}

static void shutdown_mesh_pipeline(renderer_state* state) {
    vkDestroyPipeline(state->device.handle, state->mesh_pipeline, state->allocator);
    vkDestroyPipelineLayout(state->device.handle, state->mesh_pipeline_layout, state->allocator);
    ETINFO("Mesh pipeline & pipeline layout destroyed");

    unload_shader(state, &state->mesh_fragment);
    unload_shader(state, &state->mesh_vertex);
    ETINFO("Mesh pipeline shaders unloaded.");
}

static b8 intialize_default_data(renderer_state* state) {
    vertex* vertices = etallocate(sizeof(vertex) * 4, MEMORY_TAG_RENDERER);

    vertices[0].position = (v3s){.raw = { 0.5f, -0.5f, 0.0f}};
    vertices[1].position = (v3s){.raw = { 0.5f,  0.5f, 0.0f}};
    vertices[2].position = (v3s){.raw = {-0.5f, -0.5f, 0.0f}};
    vertices[3].position = (v3s){.raw = {-0.5f,  0.5f, 0.0f}};

    vertices[0].color = (v4s){.raw = {0.0f, 0.0f, 0.0f, 1.0f}};
    vertices[1].color = (v4s){.raw = {0.9f, 0.9f, 0.9f, 1.0f}};
    vertices[2].color = (v4s){.raw = {1.0f, 0.0f, 0.0f, 1.0f}};
    vertices[3].color = (v4s){.raw = {0.0f, 1.0f, 0.0f, 1.0f}};

    // vertices[0].color = (v4s){.raw = {1.0f, 0.0f, 0.0f, 1.0f}};
    // vertices[1].color = (v4s){.raw = {1.0f, 0.0f, 0.0f, 1.0f}};
    // vertices[2].color = (v4s){.raw = {1.0f, 0.0f, 0.0f, 1.0f}};
    // vertices[3].color = (v4s){.raw = {1.0f, 0.0f, 0.0f, 1.0f}};

    u32* indices = etallocate(sizeof(u32) * 6, MEMORY_TAG_RENDERER);
    indices[0] = 0;
    indices[1] = 1;
    indices[2] = 2;
    
    indices[3] = 2;
    indices[4] = 1;
    indices[5] = 3;

    state->rectangle = upload_mesh(state, 6, indices, 4, vertices);
    
    etfree(vertices, sizeof(vertex) * 4, MEMORY_TAG_RENDERER);
    etfree(indices, sizeof(u32) * 6, MEMORY_TAG_RENDERER);

    const char* path = "build/assets/gltf/basicmesh.glb";
    state->meshes = load_gltf_meshes(path, state);
    if (!state->meshes) {
        ETERROR("Error loading file %s", path);
        return false;
    }

    return true;
}

static void shutdown_default_data(renderer_state* state) {
    for (u32 i = 0; i < dynarray_length(state->meshes); ++i) {
        buffer_destroy(state, &state->meshes[i].mesh_buffers.vertex_buffer);
        buffer_destroy(state, &state->meshes[i].mesh_buffers.index_buffer);
        state->meshes[i].mesh_buffers.vertex_buffer_address = 0;
        dynarray_destroy(state->meshes[i].surfaces);
    }
    dynarray_destroy(state->meshes);
    ETINFO("Test GLTF file unloaded");

    buffer_destroy(state, &state->rectangle.vertex_buffer);
    buffer_destroy(state, &state->rectangle.index_buffer);
    state->rectangle.vertex_buffer_address = 0;
}

static void draw_geometry(renderer_state* state, VkCommandBuffer cmd) {
    VkRenderingAttachmentInfo color_attachment = init_color_attachment_info(
        state->render_image.view, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo depth_attachment = init_depth_attachment_info(
        state->depth_image.view, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    VkExtent2D window_extent = {.width = state->width, .height = state->height};
    VkRenderingInfo render_info = init_rendering_info(window_extent, &color_attachment, &depth_attachment);

    vkCmdBeginRendering(cmd, &render_info);

    // Triangle pipeline draw

    VkViewport viewport = {0};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = state->width;
    viewport.height = state->height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {0};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = state->width;
    scissor.extent.height = state->height;

    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state->triangle_pipeline);
    vkCmdDraw(cmd, 3, 1, 0, 0);

    // Mesh pipeline draw
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state->mesh_pipeline);

    gpu_draw_push_constants push_constants = {
        .world_matrix = glms_mat4_identity(),
        .vertex_buffer = state->rectangle.vertex_buffer_address};
    vkCmdPushConstants(cmd, state->mesh_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(gpu_draw_push_constants), &push_constants);
    vkCmdBindIndexBuffer(cmd, state->rectangle.index_buffer.handle, 0, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);

    // MVP calculation: 
    m4s view = glms_translate_make((v3s){.raw = {0.f, 0.f, -5.f}});

    // TODO: Figure out projection matrix mess ups & confusion
    m4s project = glms_perspective(glm_rad(70.f), ((f32)state->width/(f32)state->height), 10000.f, 0.1f);
    project.raw[1][1] *= -1;

    m4s vp = glms_mat4_mul(project, view);

    gpu_draw_push_constants push_constants1 = {
        .world_matrix = vp,
        .vertex_buffer = state->meshes[2].mesh_buffers.vertex_buffer_address};
    vkCmdPushConstants(cmd, state->mesh_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(gpu_draw_push_constants), &push_constants1);
    vkCmdBindIndexBuffer(cmd, state->meshes[2].mesh_buffers.index_buffer.handle, 0, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexed(cmd, state->meshes[2].surfaces[0].count, 1, state->meshes[2].surfaces[0].start_index, 0, 0);

    vkCmdEndRendering(cmd);
}

void renderer_on_resize(renderer_state* state, i32 width, i32 height) {
    state->width = width;
    state->height = height;
    state->swapchain_dirty = true;
}

// TODO: Move to another separate file for handling utility functions
// TODO: Remove the VK_MEMORY_PROPERTY_HOST_COHERENT_BIT bit from the staging buffer and flush manually
// TODO:GOAL: Use queue from dedicated transfer queue family(state->device.transfer_queue) to do the transfer
gpu_mesh_buffers upload_mesh(renderer_state* state, u32 index_count, u32* indices, u32 vertex_count, vertex* vertices) {
    const u64 vertex_buffer_size = vertex_count * sizeof(vertex);
    const u64 index_buffer_size = index_count * sizeof(u32);

    gpu_mesh_buffers new_surface;

    // Create Vertex Buffer
    buffer_create(
        state,
        vertex_buffer_size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
        | VK_BUFFER_USAGE_TRANSFER_DST_BIT
        | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &new_surface.vertex_buffer);
    
    VkBufferDeviceAddressInfo device_address_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .pNext = 0,
        .buffer = new_surface.vertex_buffer.handle};
    // Get Vertex buffer address
    new_surface.vertex_buffer_address = vkGetBufferDeviceAddress(state->device.handle, &device_address_info);

    // Create Index Buffer
    buffer_create(
        state,
        index_buffer_size,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT
        | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &new_surface.index_buffer);
    
    // Create staging buffer toi transfer memory from VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT to VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    buffer staging;
    buffer_create(
        state,
        vertex_buffer_size + index_buffer_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &staging);

    void* mapped_memory;
    vkMapMemory(state->device.handle, staging.memory, 0, vertex_buffer_size + index_buffer_size, 0, &mapped_memory);

    etcopy_memory(mapped_memory, vertices, vertex_buffer_size);
    etcopy_memory((u8*)mapped_memory + vertex_buffer_size, indices, index_buffer_size);

    vkUnmapMemory(state->device.handle, staging.memory);

    // Immediate Command Buffer use start
    VK_CHECK(vkResetFences(state->device.handle, 1, &state->imm_fence));
    VK_CHECK(vkResetCommandBuffer(state->imm_buffer, 0));

    VkCommandBuffer cmd = state->imm_buffer;

    VkCommandBufferBeginInfo cmd_begin =
        init_command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmd_begin));

    /* TODO: Use VkCopyBufferInfo2 to perform this copy */
    VkBufferCopy vertex_copy = {
        .dstOffset = 0,
        .srcOffset = 0,
        .size = vertex_buffer_size};

    vkCmdCopyBuffer(cmd, staging.handle, new_surface.vertex_buffer.handle, 1, &vertex_copy);

    VkBufferCopy index_copy = {
        .dstOffset = 0,
        .srcOffset = vertex_buffer_size,
        .size = index_buffer_size};

    vkCmdCopyBuffer(cmd, staging.handle, new_surface.index_buffer.handle, 1, &index_copy);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmd_info = init_command_buffer_submit_info(cmd);
    VkSubmitInfo2 submit = init_submit_info2(0, 0, 1, &cmd_info, 0, 0);

    // submit command buffer to the queue and execute it.
    VK_CHECK(vkQueueSubmit2(state->device.graphics_queue, 1, &submit, state->imm_fence));

    VK_CHECK(vkWaitForFences(state->device.handle, 1, &state->imm_fence, VK_TRUE, 9999999999));

    buffer_destroy(state, &staging);

    return new_surface;
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

static m4s test_perspective(float vertical_fov, float aspect_ratio, float n, float f, m4s *inverse) {

}