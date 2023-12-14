#include "renderer/src/vk_types.h"
#include "renderer.h"

#include "renderer/src/utilities/vkinit.h"

#include "renderer/src/device.h"
#include "renderer/src/swapchain.h"
#include "renderer/src/image.h"
#include "renderer/src/pipeline.h"
#include "renderer/src/shader.h"
#include "renderer/src/descriptor.h"

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

static void initialize_descriptors(renderer_state* state);
static void shutdown_descriptors(renderer_state* state);

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

    // TEMP:
    state->frame_number = 0;
    // TEMP: END

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

    // Render image initialization
    VkExtent3D render_image_extent = {
        .width = state->width,
        .height = state->height,
        .depth = 1};
    image2D_create(state, 
        render_image_extent,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &state->render_image);
    ETINFO("Render image created");

    create_frame_command_structures(state);
    ETINFO("Frame command structures created.");
    create_frame_synchronization_structures(state);
    ETINFO("Frame synchronization structures created.");

    // This function initializes:
    // state->test_compute_descriptor_set_layout
    // state->test_compute_descriptor_set
    // state->global_ds_allocator
    initialize_descriptors(state);
    ETINFO("Descriptors Initialized.");

    // TEMP: Until post processing effect structure/system is created/becomes more robust. 
    load_shader(state, "build/assets/shaders/gradient.comp.spv", &state->test_compute_shader);

    VkPipelineLayoutCreateInfo compute_effect_pipeline_layout_info = init_pipeline_layout_create_info();
    compute_effect_pipeline_layout_info.setLayoutCount = 1;
    compute_effect_pipeline_layout_info.pSetLayouts = &state->test_compute_descriptor_set_layout;

    // Push constant structure for compute effect pipelines
    VkPushConstantRange push_constant = {0};
    push_constant.offset = 0;
    push_constant.size = sizeof(compute_push_constants);
    push_constant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    compute_effect_pipeline_layout_info.pushConstantRangeCount = 1;
    compute_effect_pipeline_layout_info.pPushConstantRanges = &push_constant;
    
    VK_CHECK(vkCreatePipelineLayout(state->device.handle, &compute_effect_pipeline_layout_info, state->allocator, &state->test_compute_effect.layout));

    VkPipelineShaderStageCreateInfo compute_effect_pipeline_shader_stage_info = init_pipeline_shader_stage_create_info();
    compute_effect_pipeline_shader_stage_info.stage = state->test_compute_shader.stage;
    compute_effect_pipeline_shader_stage_info.module = state->test_compute_shader.module;
    compute_effect_pipeline_shader_stage_info.pName = state->test_compute_shader.entry_point;

    VkComputePipelineCreateInfo compute_effect_info = init_compute_pipeline_create_info();
    compute_effect_info.layout = state->test_compute_effect.layout;
    compute_effect_info.stage = compute_effect_pipeline_shader_stage_info;

    VK_CHECK(vkCreateComputePipelines(state->device.handle, VK_NULL_HANDLE, 1, &compute_effect_info, state->allocator, &state->test_compute_effect.pipeline));
    ETINFO("Test compute effect created.");
    // TEMP: END

    // TEMP: Until material framework is stood up
    load_shader(state, "build/assets/shaders/test1.vert.spv", &state->test_graphics_vertex);
    load_shader(state, "build/assets/shaders/test1.frag.spv", &state->test_graphics_fragment);

    pipeline_builder graphics_builder = pipeline_builder_create();
    pipeline_builder_set_shaders(&graphics_builder,
        state->test_graphics_vertex,
        state->test_graphics_fragment);
    pipeline_builder_set_input_topology(&graphics_builder, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipeline_builder_set_polygon_mode(&graphics_builder, VK_POLYGON_MODE_FILL);
    pipeline_builder_set_cull_mode(&graphics_builder, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipeline_builder_set_multisampling_none(&graphics_builder);
    pipeline_builder_disable_blending(&graphics_builder);
    pipeline_builder_disable_depthtest(&graphics_builder);
    pipeline_builder_set_color_attachment_format(&graphics_builder, state->render_image.format);
    pipeline_builder_set_depth_format(&graphics_builder, VK_FORMAT_UNDEFINED);

    VkPipelineLayoutCreateInfo layout_info = init_pipeline_layout_create_info();
    VK_CHECK(vkCreatePipelineLayout(state->device.handle, &layout_info, state->allocator, &state->test_graphics_pipeline_layout));

    graphics_builder.layout = state->test_graphics_pipeline_layout;

    state->test_graphics_pipeline = pipeline_builder_build(&graphics_builder, state);

    pipeline_builder_destroy(&graphics_builder);
    // TEMP: END

    *out_state = state;
    return true;
}

void renderer_shutdown(renderer_state* state) {
    vkDeviceWaitIdle(state->device.handle);
    // Destroy reverse order of creation

    // TEMP: Until material framework is stood up
    vkDestroyPipeline(state->device.handle, state->test_graphics_pipeline, state->allocator);
    vkDestroyPipelineLayout(state->device.handle, state->test_graphics_pipeline_layout, state->allocator);
    ETINFO("Test graphics pipeline & test graphics pipeline layout destroyed.");

    unload_shader(state, &state->test_graphics_vertex);
    unload_shader(state, &state->test_graphics_fragment);
    ETINFO("Test graphics vertex and fragment shaders unloaded.");
    // TEMP: Until material framework is stood up

    // TEMP: Until post processing effect structure/system is created/becomes more robust. 
    vkDestroyPipeline(state->device.handle, state->test_compute_effect.pipeline, state->allocator);
    vkDestroyPipelineLayout(state->device.handle, state->test_compute_effect.layout, state->allocator);
    ETINFO("Test compute effect pipeline & test compute effect pipeline layout destroyed.");

    unload_shader(state, &state->test_compute_shader);
    ETINFO("Test compute shader unloaded.");
    // TEMP: Until post processing effect structure/system is created/becomes more robust. 

    shutdown_descriptors(state);
    ETINFO("Descriptors shutdown.");

    destroy_frame_synchronization_structures(state);
    ETINFO("Frame synchronization structures destroyed.");

    destroy_frame_command_structures(state);
    ETINFO("Frame command structures destroyed.");

    image2D_destroy(state, &state->render_image);
    ETINFO("Render image destroyed.");

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
    image_barrier(frame_cmd, state->render_image.handle,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
        VK_ACCESS_2_TRANSFER_READ_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT , VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

    // TEMP: Hardcode test compute
    vkCmdBindPipeline(frame_cmd, VK_PIPELINE_BIND_POINT_COMPUTE, state->test_compute_effect.pipeline);

    vkCmdBindDescriptorSets(frame_cmd, VK_PIPELINE_BIND_POINT_COMPUTE, state->test_compute_effect.layout, 0, 1, &state->test_compute_descriptor_set, 0, 0);

    compute_push_constants pc = {
        .data1 = (v4s){1.0f, 0.0f, 0.0f, 1.0f},
        .data2 = (v4s){0.0f, 0.0f, 1.0f, 1.0f},
    };

    vkCmdPushConstants(frame_cmd, state->test_compute_effect.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(compute_push_constants), &pc);

    vkCmdDispatch(frame_cmd, ceil(state->width / 16.0f), ceil(state->height / 16.0f), 1);
    // TEMP: END

    image_barrier(frame_cmd, state->render_image.handle,
        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_ACCESS_2_SHADER_WRITE_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

    // TEMP: Hardcode test graphics
    VkRenderingAttachmentInfo color_attachment = init_rendering_attachment_info(state->render_image.view, 0, VK_IMAGE_LAYOUT_GENERAL);

    VkExtent2D window_extent = {.width = state->width, .height = state->height};
    VkRenderingInfo render_info = init_rendering_info(window_extent, &color_attachment, 0);

    vkCmdBeginRendering(frame_cmd, &render_info);

    vkCmdBindPipeline(frame_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state->test_graphics_pipeline);

    VkViewport viewport = {0};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = state->width;
    viewport.height = state->height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vkCmdSetViewport(frame_cmd, 0, 1, &viewport);

    VkRect2D scissor = {0};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = state->width;
    scissor.extent.height = state->height;

    vkCmdSetScissor(frame_cmd, 0, 1, &scissor);

    vkCmdDraw(frame_cmd, 3, 1, 0, 0);

    vkCmdEndRendering(frame_cmd);
    // TEMP: END

    // Image barrier to transition render image to transfer source optimal layout
    image_barrier(frame_cmd, state->render_image.handle,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_2_TRANSFER_READ_BIT,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT);

    // Image barrier to transition swapchain image to transfer destination optimal
    image_barrier(frame_cmd, state->swapchain_images[swapchain_index],
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
    image_barrier(frame_cmd, state->swapchain_images[swapchain_index],
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
    state->test_compute_descriptor_set_layout = descriptor_set_layout_builder_build(&dsl_builder, state);
    ETINFO("Test Compute Descriptor Set Layout created.");

    state->test_compute_descriptor_set = descriptor_set_allocator_allocate(&state->global_ds_allocator, state->test_compute_descriptor_set_layout, state);
    descriptor_set_layout_builder_destroy(&dsl_builder);
    ETINFO("Test Compute Descriptor Set allocated");

    ds_writer writer = descriptor_set_writer_create_initialize();
    descriptor_set_writer_write_image(&writer, 0, state->render_image.view, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    descriptor_set_writer_update_set(&writer, state->test_compute_descriptor_set, state);
    descriptor_set_writer_shutdown(&writer);
    ETINFO("Test Compute Descriptor Set updated.");
}

static void shutdown_descriptors(renderer_state* state) {
    descriptor_set_allocator_shutdown(&state->global_ds_allocator, state);
    ETINFO("Descriptor Set Allocator destroyed.");

    vkDestroyDescriptorSetLayout(state->device.handle, state->test_compute_descriptor_set_layout, state->allocator);
    ETINFO("Test compute descriptor set layout destroyed.");
}

void renderer_on_resize(renderer_state* state, i32 width, i32 height) {
    state->width = width;
    state->height = height;
    state->swapchain_dirty = true;
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