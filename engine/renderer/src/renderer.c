#include "renderer/rendererAPI.h"
#include "renderer.h"

#include "data_structures/dynarray.h"

#include "core/logger.h"
#include "core/etstring.h"
#include "memory/etmemory.h"

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

#include "window/renderer_window.h"

static void initialize_immediate_submit(renderer_state* state);
static void shutdown_immediate_submit(renderer_state* state);

// TODO: Move to scene renderer, more programmable
static void initialize_descriptors(renderer_state* state);
static void shutdown_descriptors(renderer_state* state);

static b8 initialize_compute_effects(renderer_state* state);
static void shutdown_compute_effects(renderer_state* state);
// TODO: END

static b8 initialize_default_data(renderer_state* state);
static void shutdown_default_data(renderer_state* state);

VkBool32 vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData);

// NOTE: Exiting this function with return value false does not always free all allocated memory.
// This is fine for now as a return value of false is unrecoverable.
// But if there is a situation in the future where we run the function again to try again
// then we will be leaking memory 
b8 renderer_initialize(renderer_state** out_state, renderer_config config) {
    renderer_state* state = etallocate(sizeof(renderer_state), MEMORY_TAG_RENDERER);
    etzero_memory(state, sizeof(renderer_state));
    
    state->allocator = 0;
    state->swapchain.swapchain = VK_NULL_HANDLE;

    VkApplicationInfo app_cinfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = config.app_name,
        .applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
        .pEngineName = config.engine_name,
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
    ETDEBUG("Vulkan debug messenger created.");
#endif

    // NOTE: Remove references to surface & swapchain from code in this block
    if (!window_create_vulkan_surface(state, config.window)) {
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
    initialize_swapchain(state, &state->swapchain);

    // TEMP: HACK: TODO: Make render image resolution configurable from engine & not the window_extent
    VkExtent3D render_resolution = {
        .width = state->swapchain.image_extent.width,
        .height = state->swapchain.image_extent.height,
        .depth = 1};
    state->render_extent = render_resolution;
    // TEMP: HACK: TODO: END

    // Rendering attachment images initialization
    
    // Color attachment
    VkImageUsageFlags draw_image_usages = 0;
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
    SET_DEBUG_NAME(state, VK_OBJECT_TYPE_IMAGE, state->render_image.handle, "MainRenderImage");
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
    SET_DEBUG_NAME(state, VK_OBJECT_TYPE_IMAGE, state->depth_image.handle, "MainDepthImage");
    ETINFO("Depth image created");

    initialize_immediate_submit(state);

    initialize_descriptors(state);

    if (!initialize_compute_effects(state)) {
        ETERROR("Could not initialize compute effects.");
        return false;
    }

    if (!initialize_default_data(state)) {
        ETFATAL("Error intializing data.");
        return false;
    }

    *out_state = state;
    // NOTE: END (Remove surface & swapchain references)
    return true;
}

void renderer_shutdown(renderer_state* state) {
    vkDeviceWaitIdle(state->device.handle);
    // Destroy reverse order of creation

    shutdown_default_data(state);

    shutdown_compute_effects(state);

    shutdown_descriptors(state);

    shutdown_immediate_submit(state);

    image_destroy(state, &state->depth_image);
    image_destroy(state, &state->render_image);
    ETINFO("Rendering attachments (color, depth) destroyed.");

    shutdown_swapchain(state, &state->swapchain);

    device_destroy(state, &state->device);
    
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

static void initialize_immediate_submit(renderer_state* state) {
    // Immediate command pool & buffer
    VkCommandPoolCreateInfo imm_pool_info = init_command_pool_create_info(
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        state->device.graphics_qfi);
    VK_CHECK(vkCreateCommandPool(
        state->device.handle,
        &imm_pool_info,
        state->allocator,
        &state->imm_pool));
    SET_DEBUG_NAME(state, VK_OBJECT_TYPE_COMMAND_POOL, state->imm_pool, "ImmediateSubmissionCommandPool");

    VkCommandBufferAllocateInfo imm_buffer_alloc_info = init_command_buffer_allocate_info(
        state->imm_pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);
    VK_CHECK(vkAllocateCommandBuffers(
        state->device.handle,
        &imm_buffer_alloc_info,
        &state->imm_buffer));
    SET_DEBUG_NAME(state, VK_OBJECT_TYPE_COMMAND_BUFFER, state->imm_buffer, "ImmediateSubmissionCommandBuffer");

    // Immediate Fence
    VkFenceCreateInfo fence_info = init_fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    VK_CHECK(vkCreateFence(state->device.handle, &fence_info, state->allocator, &state->imm_fence));
    SET_DEBUG_NAME(state, VK_OBJECT_TYPE_FENCE, state->imm_fence, "ImmediateSubmissionFence");

    ETINFO("Immediate submission functionality initialized.");
}

static void shutdown_immediate_submit(renderer_state* state) {
    vkDestroyCommandPool(state->device.handle, state->imm_pool, state->allocator);
    vkDestroyFence(state->device.handle, state->imm_fence, state->allocator); 
    ETINFO("Immediate submission functionality shutdown.");
}

static void initialize_descriptors(renderer_state* state) {
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
    ETINFO("Descriptors Initialized");
}

// TODO: Should be reverse order of creation just for uniformity
static void shutdown_descriptors(renderer_state* state) {
    descriptor_set_allocator_shutdown(&state->global_ds_allocator, state);
    vkDestroyDescriptorSetLayout(state->device.handle, state->scene_data_descriptor_set_layout, state->allocator);
    vkDestroyDescriptorSetLayout(state->device.handle, state->draw_image_descriptor_set_layout, state->allocator);
    ETINFO("Descriptors shutdown");
}

static b8 initialize_compute_effects(renderer_state* state) {
    if (!load_shader(state, "assets/shaders/gradient.comp.spv", &state->gradient_shader)) {
        ETERROR("Error loading compute effect shader.");
        return false;
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
    ETINFO("Compute effects shutdown.");
}

static b8 initialize_default_data(renderer_state* state) {
    // NOTE: For some reason the u32 is being interpreted as 
    // 0xFFFFFFFF -- aabbggrr. Endianness doing something here
    u32 white = 0xFFFFFFFF;
    image2D_create_data(
        state,
        &white,
        (VkExtent3D){.width = 1, .height = 1, .depth = 1},
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &state->default_white);
    ETINFO("White default image created.");

    u32 grey = 0xFFAAAAAA;
    image2D_create_data(
        state,
        &grey,
        (VkExtent3D){.width = 1, .height = 1, .depth = 1},
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &state->default_grey);
    ETINFO("Grey default image created.");

    u32 black = 0xFF000000;
    image2D_create_data(
        state,
        &black,
        (VkExtent3D){.width = 1, .height = 1, .depth = 1},
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &state->default_black);
    ETINFO("Black default image created.");

    u32 normal = 0xFFFF7F7F;
    image2D_create_data(
        state,
        &normal,
        (VkExtent3D){.width = 1, .height = 1, .depth = 1},
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &state->default_normal);
    ETINFO("Normal default image created.");

    u32 magenta = 0xFFFF00FF;
    u32 pixels[16 * 16] = {0};
    for (u32 i = 0; i < 16; ++i) {
        for (u32 j = 0; j < 16; ++j) {
            pixels[j * 16 + i] = ((i % 2) ^ (j % 2)) ? magenta : black;
        }
    }
    image2D_create_data(
        state,
        &pixels[0],
        (VkExtent3D){.width = 16, .height = 16, .depth = 1},
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &state->default_error);
    ETINFO("Checkerboard default image created.");


    VkSamplerCreateInfo sampler_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .maxLod = VK_LOD_CLAMP_NONE,
        .minLod = 0,
    };
    sampler_info.magFilter = VK_FILTER_NEAREST;
    sampler_info.minFilter = VK_FILTER_NEAREST;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    vkCreateSampler(state->device.handle, &sampler_info, state->allocator, &state->nearest_smpl);

    sampler_info.magFilter = VK_FILTER_LINEAR;
    sampler_info.minFilter = VK_FILTER_LINEAR;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    vkCreateSampler(state->device.handle, &sampler_info, state->allocator, &state->linear_smpl);
    return true;
}

static void shutdown_default_data(renderer_state* state) {
    vkDestroySampler(state->device.handle, state->linear_smpl, state->allocator);
    vkDestroySampler(state->device.handle, state->nearest_smpl, state->allocator);

    image_destroy(state, &state->default_normal);
    image_destroy(state, &state->default_error);
    image_destroy(state, &state->default_black);
    image_destroy(state, &state->default_grey);
    image_destroy(state, &state->default_white);
}

VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData
) {
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