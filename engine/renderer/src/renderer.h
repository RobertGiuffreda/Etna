#pragma once
#include "defines.h"

#include "renderer/src/vk_types.h"
#include "renderer/src/swapchain.h"
#include "renderer/src/shader.h"

typedef struct renderer_state {
    VkInstance instance;
#ifdef _DEBUG
    VkDebugUtilsMessengerEXT debug_messenger;
    PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT;
#endif
    VkAllocationCallbacks* allocator;

    device device;

    // TODO: Move to window
    swapchain swapchain;
    // TODO: END

    // TODO: Move to scene
    VkExtent3D render_extent;
    image render_image;
    image depth_image;
    // TODO: END
    
    // TEMP: Compute effect members, will be removed.
    ds_allocator global_ds_allocator;
    
    shader gradient_shader;
    compute_effect gradient_effect;

    VkDescriptorSetLayout draw_image_descriptor_set_layout;
    VkDescriptorSet draw_image_descriptor_set;

    VkDescriptorSetLayout scene_data_descriptor_set_layout;
    // TEMP: END

    // NOTE: Immediate Submission
    VkCommandPool imm_pool;
    VkCommandBuffer imm_buffer;
    VkFence imm_fence;

    // NOTE: Defaults
    image default_white;
    image default_black;
    image default_grey;
    image default_error;
    image default_normal;

    VkSampler linear_smpl;
    VkSampler nearest_smpl;
} renderer_state;

// TODO: Setup debug names for vulkan objects
#ifdef _DEBUG
b8 renderer_set_debug_object_name(renderer_state* state, VkObjectType object_type, u64 object_handle, const char* object_name);
#define SET_DEBUG_NAME(render_state, object_type, object_handle, object_name) renderer_set_debug_object_name(render_state, object_type, (u64)(object_handle), object_name)
#else
#define SET_DEBUG_NAME(render_state, object_type, object_handle, object_name)
#endif
