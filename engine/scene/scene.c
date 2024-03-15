#include "scene.h"
#include "scene/scene_private.h"

#include "data_structures/dynarray.h"

#include "core/etstring.h"
#include "core/logger.h"

// TEMP: Until events refactor
#include "core/events.h"
#include "core/input.h"
// TEMP: END

#include "memory/etmemory.h"

#include "resources/image_manager.h"
#include "resources/resource_private.h"
#include "resources/material.h"

#include "renderer/src/vk_types.h"
#include "renderer/src/renderer.h"
#include "renderer/src/utilities/vkinit.h"
#include "renderer/src/image.h"
#include "renderer/src/buffer.h"
#include "renderer/src/shader.h"
#include "renderer/src/pipeline.h"
#include "resources/importers/gltfimporter.h"

// TEMP: Until a math library is situated
#include <math.h>
// TEMP: END

// TEMP: Until events refactor
static b8 scene_on_key_event(u16 code, void* scne, event_data data);
// TEMP: END

// TODO: Make separate scene renderer struct
static void scene_renderer_init(scene* scene, renderer_state* state);
static void scene_renderer_shutdown(scene* scene);

static void scene_renderer_descriptors_init(scene* scene, renderer_state* state);
static void scene_renderer_descriptors_shutdown(scene* scene, renderer_state* state);
// TODO: END

b8 scene_initalize(scene** scn, renderer_state* state) {
    scene* new_scene = etallocate(sizeof(struct scene), MEMORY_TAG_SCENE);
    new_scene->state = state;
    new_scene->name = "Etna Scene Testing";

    camera_create(&new_scene->cam);
    new_scene->cam.position = (v3s){.raw = {0.0f, 0.0f, 5.0f}};

    // NOTE: This will be passed a config when serialization is implemented
    v4s a_color = { .raw = {.1f, .1f, .1f, 3.f}};
    v4s l_color = { .raw = {1.f, 1.f, 1.f, 50.f}};
    
    new_scene->data.ambient_color = a_color;
    new_scene->data.light_color = l_color;

    scene_renderer_init(new_scene, state);
    
    event_observer_register(EVENT_CODE_KEY_RELEASE, new_scene, scene_on_key_event);
    *scn = new_scene;
    return true;
}

void scene_shutdown(scene* scene) {
    renderer_state* state = scene->state;

    event_observer_deregister(EVENT_CODE_KEY_RELEASE, scene, scene_on_key_event);

    // NOTE: This is needed to make sure objects are not destroyed while in use
    vkDeviceWaitIdle(state->device.handle);

    // TODO: Update to GPU driven
    image_manager_shutdown(scene->image_bank);
    // TODO: END

    for (u32 i = 0; i < scene->sampler_count; ++i)
        vkDestroySampler(state->device.handle, scene->samplers[i], state->allocator);
    etfree(scene->samplers, sizeof(VkSampler) * scene->sampler_count, MEMORY_TAG_SCENE);

    scene_renderer_shutdown(scene);

    camera_destroy(&scene->cam);
    
    etfree(scene, sizeof(struct scene), MEMORY_TAG_SCENE);
}

// TEMP: For testing and debugging
static f32 light_offset = 2.0f;
static b8 light_dynamic = true;
// TEMP: END

void scene_update(scene* scene, f64 dt) {
    renderer_state* state = scene->state;

    camera_update(&scene->cam, dt);

    scene->data.max_draw_count = MAX_DRAW_COMMANDS;

    // TODO: Camera should store near and far values & calculate perspective matrix
    // TODO: Scene should register for event system and update camera stuff itself 
    m4s view = camera_get_view_matrix(&scene->cam);
    m4s project = glms_perspective(
        /* fovy */ glm_rad(70.f),
        ((f32)state->swapchain.image_extent.width/(f32)state->swapchain.image_extent.height), 
        /* near-z: */ 10000.f,
        /* far-z: */ 0.1f);

    // invert the Y direction on projection matrix so that we are more similar
    // to opengl and gltf axis
    project.raw[1][1] *= -1;

    v4s l_pos = glms_vec4(scene->cam.position, 1.0f);
    if (light_dynamic) {
        scene->data.light_position = l_pos;
        scene->data.light_position.y += light_offset;
    }

    m4s vp = glms_mat4_mul(project, view);
    scene->data.view = view;
    scene->data.proj = project;
    scene->data.viewproj = vp;
    
    scene->data.view_pos = glms_vec4(scene->cam.position, 1.0f);
}

void scene_renderer_init(scene* scene, renderer_state* state) {
    image_manager_initialize(&scene->image_bank, state);

    scene->render_fences = etallocate(
        sizeof(VkFence) * state->swapchain.image_count,
        MEMORY_TAG_SCENE);
    scene->graphics_pools = etallocate(
        sizeof(VkCommandPool) * state->swapchain.image_count,
        MEMORY_TAG_SCENE);
    scene->graphics_command_buffers = etallocate(
        sizeof(VkCommandBuffer) * state->swapchain.image_count,
        MEMORY_TAG_SCENE);
    VkFenceCreateInfo fence_info = init_fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    VkCommandPoolCreateInfo gpool_info = init_command_pool_create_info(
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        state->device.graphics_qfi);
    for (u32 i = 0; i < state->swapchain.image_count; ++i) {
    }
    for (u32 i = 0; i < state->swapchain.image_count; ++i) {
        DEBUG_BLOCK(
            char fence_name[] = "RenderFence X";
            fence_name[str_length(fence_name) - 1] = i - '0';
            char cmd_pool_name[] = "GraphicsCommandPool X";
            cmd_pool_name[str_length(cmd_pool_name) - 1] = i - '0';
            char cmd_buff_name[] = "GraphicsCommandBuffer X";
            cmd_buff_name[str_length(cmd_buff_name) - 1] = i - '0';
        );
        VK_CHECK(vkCreateFence(
            state->device.handle,
            &fence_info,
            state->allocator,
            &scene->render_fences[i]));
        SET_DEBUG_NAME(state, VK_OBJECT_TYPE_FENCE, scene->render_fences[i], fence_name);
        VK_CHECK(vkCreateCommandPool(state->device.handle,
            &gpool_info,
            state->allocator,
            &scene->graphics_pools[i]));
        SET_DEBUG_NAME(state, VK_OBJECT_TYPE_COMMAND_POOL, scene->graphics_pools[i], cmd_pool_name);
        VkCommandBufferAllocateInfo command_buffer_alloc_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = scene->graphics_pools[i],
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        VK_CHECK(vkAllocateCommandBuffers(
            state->device.handle,
            &command_buffer_alloc_info,
            &scene->graphics_command_buffers[i]));
        SET_DEBUG_NAME(state, VK_OBJECT_TYPE_COMMAND_BUFFER, scene->graphics_command_buffers[i], cmd_buff_name);
    }

    // Create buffers
    buffer_create(
        state,
        sizeof(scene_data),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &scene->scene_uniforms);
    SET_DEBUG_NAME(state, VK_OBJECT_TYPE_BUFFER, scene->scene_uniforms.handle, "SceneFrameUniformBuffer");
    buffer_create(
        state,
        sizeof(object) * MAX_OBJECTS,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &scene->object_buffer);
    SET_DEBUG_NAME(state, VK_OBJECT_TYPE_BUFFER, scene->object_buffer.handle, "SceneObjectBuffer");
    buffer_create(
        state,
        sizeof(u32) * MAT_PIPE_MAX,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &scene->counts_buffer);
    SET_DEBUG_NAME(state, VK_OBJECT_TYPE_BUFFER, scene->counts_buffer.handle, "PipelineDrawCountsBuffer");
    buffer_create(
        state,
        sizeof(VkDeviceAddress) * MAT_PIPE_MAX,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &scene->draws_buffer);
    SET_DEBUG_NAME(state, VK_OBJECT_TYPE_BUFFER, scene->draws_buffer.handle, "PipelineDrawBufferPointersBuffer");

    scene_renderer_descriptors_init(scene, state);

    const char* draw_gen_path = "assets/shaders/draws.comp.spv.opt";
    shader draw_gen;
    if (!load_shader(state, draw_gen_path, &draw_gen)) {
        ETFATAL("Unable to load draw generation shader.");
        return;
    }
    VkPipelineLayoutCreateInfo draw_gen_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = 0,
        .setLayoutCount = 1,
        .pSetLayouts = &scene->scene_set_layout};
    VK_CHECK(vkCreatePipelineLayout(
        state->device.handle,
        &draw_gen_layout_info,
        state->allocator,
        &scene->draw_gen_layout));
    SET_DEBUG_NAME(state, VK_OBJECT_TYPE_PIPELINE_LAYOUT, scene->draw_gen_layout, "DrawGenerationPipelineLayout");
    VkPipelineShaderStageCreateInfo draw_stage_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = 0,
        .pName = draw_gen.entry_point,
        .stage = draw_gen.stage,
        .module = draw_gen.module};
    VkComputePipelineCreateInfo draw_pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = 0,
        .layout = scene->draw_gen_layout,
        .stage = draw_stage_info};
    VK_CHECK(vkCreateComputePipelines(
        state->device.handle,
        VK_NULL_HANDLE,
        /* CreateInfoCount */ 1,
        &draw_pipeline_info,
        state->allocator,
        &scene->draw_gen_pipeline));
    SET_DEBUG_NAME(state, VK_OBJECT_TYPE_PIPELINE, scene->draw_gen_pipeline, "DrawGenerationPipeline");
    unload_shader(state, &draw_gen);

    // Create Pipeline for bindless shaders
    VkDescriptorSetLayout pipeline_ds_layouts[] = {
        [0] = scene->scene_set_layout,
        [1] = scene->mat_set_layout,
    };
    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = 0,
        .setLayoutCount = 2,
        .pSetLayouts = pipeline_ds_layouts,
    };
    VK_CHECK(vkCreatePipelineLayout(
        state->device.handle,
        &pipeline_layout_create_info,
        state->allocator,
        &scene->mat_pipeline_layout));
    SET_DEBUG_NAME(state, VK_OBJECT_TYPE_PIPELINE_LAYOUT, scene->mat_pipeline_layout, "SharedMaterialPipelineLayout");

    // Initialize materials and material pipelines
    void* draw_buffer_addresses;
    VK_CHECK(vkMapMemory(
        state->device.handle,
        scene->draws_buffer.memory,
        /* offset */ 0,
        VK_WHOLE_SIZE,
        /* flags */ 0,
        &draw_buffer_addresses));
    // Initialize the material pipeline container structs
    for (u32 i = 0; i < MAT_PIPE_MAX; ++i) {
        mat_init(&scene->materials[i], scene, state, &scene_mat_configs[i]);
        VkDeviceAddress mat_draws_addr = buffer_get_address(state, &scene->materials[i].draws_buffer);
        etcopy_memory((u8*)draw_buffer_addresses + i * sizeof(VkDeviceAddress), &mat_draws_addr, sizeof(VkDeviceAddress));
    }
    vkUnmapMemory(state->device.handle, scene->draws_buffer.memory);
}

void scene_renderer_shutdown(scene* scene) {
    renderer_state* state = scene->state;
    dynarray_destroy(scene->transforms);
    dynarray_destroy(scene->meshes);
    dynarray_destroy(scene->surfaces);
    dynarray_destroy(scene->indices);
    dynarray_destroy(scene->vertices);

    dynarray_destroy(scene->objects);
    dynarray_destroy(scene->geometries);

    buffer_destroy(state, &scene->index_buffer);
    buffer_destroy(state, &scene->scene_uniforms);
    buffer_destroy(state, &scene->counts_buffer);
    buffer_destroy(state, &scene->draws_buffer);
    buffer_destroy(state, &scene->object_buffer);
    buffer_destroy(state, &scene->geometry_buffer);
    buffer_destroy(state, &scene->vertex_buffer);
    buffer_destroy(state, &scene->transform_buffer);

    vkDestroyPipeline(state->device.handle, scene->draw_gen_pipeline, state->allocator);
    vkDestroyPipelineLayout(state->device.handle, scene->draw_gen_layout, state->allocator);
    vkDestroyPipelineLayout(state->device.handle, scene->mat_pipeline_layout, state->allocator);

    scene_renderer_descriptors_shutdown(scene, state);

    for (u32 i = 0; i < MAT_PIPE_MAX; ++i) {
        mat_shutdown(&scene->materials[i], scene, state);
    }

    u32 frame_overlap = state->swapchain.image_count;
    for (u32 i = 0; i < frame_overlap; ++i) {
        vkDestroyCommandPool(state->device.handle, scene->graphics_pools[i], state->allocator);
        vkDestroyFence(state->device.handle, scene->render_fences[i], state->allocator);
    }
    etfree(scene->graphics_command_buffers, sizeof(VkCommandBuffer) * frame_overlap, MEMORY_TAG_SCENE);
    etfree(scene->graphics_pools, sizeof(VkCommandPool) * frame_overlap, MEMORY_TAG_SCENE);
    etfree(scene->render_fences, sizeof(VkFence) * frame_overlap, MEMORY_TAG_SCENE);
}

b8 scene_render(scene* scene) {
    renderer_state* state = scene->state;
    // TEMP:TODO: Create staging buffer to move this instead of vkCmdUpdateBuffer
    VkCommandBuffer cmd = scene->graphics_command_buffers[state->swapchain.frame_index];
    vkCmdUpdateBuffer(cmd,
        scene->scene_uniforms.handle,
        /* Offset: */ 0,
        sizeof(scene_data),
        &scene->data);
    vkCmdFillBuffer(cmd,
        scene->counts_buffer.handle,
        /* Offset: */ 0,
        sizeof(u32) * MAT_PIPE_MAX,
        (u32)0);
    buffer_barrier(cmd, scene->scene_uniforms.handle, /* Offset: */ 0, VK_WHOLE_SIZE,
        VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);
    buffer_barrier(cmd, scene->counts_buffer.handle, /* Offset: */ 0, VK_WHOLE_SIZE,
        VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
    // TEMP:TODO: END
    return true;
}

b8 scene_frame_begin(scene* scene, renderer_state* state) {
    VkResult result;

    // Wait for the current frame to end rendering by waiting on its render fence
    result = vkWaitForFences(
        state->device.handle,
        /* Fence count: */ 1,
        &scene->render_fences[state->swapchain.frame_index],
        VK_TRUE,
        1000000000);
    VK_CHECK(result);
    
    result = vkAcquireNextImageKHR(
        state->device.handle,
        state->swapchain.swapchain,
        0xFFFFFFFFFFFFFFFF,
        state->swapchain.image_acquired[state->swapchain.frame_index],
        VK_NULL_HANDLE,
        &state->swapchain.image_index);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        VK_CHECK(vkDeviceWaitIdle(state->device.handle));
        
        // NOTE: VK_SUBOPTIMAL_KHR means an image will be successfully
        // acquired. Signalling the semaphore current semaphore when it is. 
        // This unsignals the semaphore by recreating it.
        if (result == VK_SUBOPTIMAL_KHR) {
            vkDestroySemaphore(
                state->device.handle,
                state->swapchain.image_acquired[state->swapchain.frame_index],
                state->allocator);
            VkSemaphoreCreateInfo info = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
            vkCreateSemaphore(
                state->device.handle,
                &info,
                state->allocator,
                &state->swapchain.image_acquired[state->swapchain.frame_index]
            );
        }
        recreate_swapchain(state, &state->swapchain);
        return false;
    } else VK_CHECK(result);

    // Reset the render fence for reuse
    VK_CHECK(vkResetFences(
        state->device.handle,
        /* Fence count: */ 1,
        &scene->render_fences[state->swapchain.frame_index]
    ));

    VK_CHECK(vkResetCommandBuffer(scene->graphics_command_buffers[state->swapchain.frame_index], 0));
    VkCommandBufferBeginInfo begin_info = init_command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(scene->graphics_command_buffers[state->swapchain.frame_index], &begin_info));
    return true;
}

void draw_geometry(renderer_state* state, scene* scene, VkCommandBuffer cmd) {
    u32 object_count = dynarray_length(scene->objects);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, scene->draw_gen_pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, scene->draw_gen_layout, 0, 1, &scene->scene_set, 0, NULL);
    vkCmdDispatch(cmd, ceil(object_count / 32.0f), 1, 1);

    // NOTE: Clean this up
    VkBufferMemoryBarrier2 mat_draws_buffer_barriers[MAT_PIPE_MAX];
    for (u32 i = 0; i < MAT_PIPE_MAX; ++i) {
        mat_draws_buffer_barriers[i] = buffer_memory_barrier(
            scene->materials[i].draws_buffer.handle, /* Offset */0, VK_WHOLE_SIZE,
            VK_ACCESS_2_SHADER_WRITE_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT
        );
    }
    VkDependencyInfo buffer_barriers = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .bufferMemoryBarrierCount = MAT_PIPE_MAX,
        .pBufferMemoryBarriers = mat_draws_buffer_barriers,
    };
    vkCmdPipelineBarrier2(cmd, &buffer_barriers);
    buffer_barrier(cmd, scene->counts_buffer.handle, /* offset: */ 0, VK_WHOLE_SIZE,
        VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT);
    // NOTE: END

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
    
    vkCmdBindIndexBuffer(cmd, scene->index_buffer.handle, 0, VK_INDEX_TYPE_UINT32);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, scene->mat_pipeline_layout, 0, 1, &scene->scene_set, 0, NULL);

    for (u32 i = 0; i < MAT_PIPE_MAX; ++i) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, scene->materials[i].pipe);

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, scene->mat_pipeline_layout, 1, 1, &scene->materials[i].set, 0, NULL);
        
        vkCmdDrawIndexedIndirectCount(cmd,
            scene->materials[i].draws_buffer.handle,
            /* Offset: */ 0,
            scene->counts_buffer.handle,
            sizeof(u32) * i,
            MAX_DRAW_COMMANDS,
            sizeof(draw_command)
        );
    }
    vkCmdEndRendering(cmd);
}

b8 scene_frame_end(scene* scene, renderer_state* state) {
    VkResult result;
    VkCommandBuffer frame_cmd = scene->graphics_command_buffers[state->swapchain.frame_index];

    // TEMP: Hardcode Gradient compute
    image_barrier(frame_cmd, state->render_image.handle, state->render_image.aspects,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
        VK_ACCESS_2_NONE, VK_ACCESS_2_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_NONE, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

    vkCmdBindPipeline(frame_cmd, VK_PIPELINE_BIND_POINT_COMPUTE, state->gradient_effect.pipeline);

    vkCmdBindDescriptorSets(frame_cmd, VK_PIPELINE_BIND_POINT_COMPUTE, state->gradient_effect.layout, 0, 1, &state->draw_image_descriptor_set, 0, 0);

    compute_push_constants pc = {
        .data1 = (v4s){1.0f, 0.0f, 0.0f, 1.0f},
        .data2 = (v4s){0.0f, 0.0f, 1.0f, 1.0f},
    };

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

    draw_geometry(state, scene, frame_cmd);

    // Make render image optimal layout for transfer source to swapchain image
    image_barrier(frame_cmd, state->render_image.handle, state->render_image.aspects,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT, VK_ACCESS_2_TRANSFER_READ_BIT,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT);

    // Make swapchain image optimal for recieving render image data
    image_barrier(frame_cmd, state->swapchain.images[state->swapchain.image_index], VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_ACCESS_NONE, VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT);

    // Copy render image to swapchain image
    blit_image2D_to_image2D(
        frame_cmd,
        state->render_image.handle,
        state->swapchain.images[state->swapchain.image_index],
        state->render_extent,
        state->swapchain.image_extent,
        VK_IMAGE_ASPECT_COLOR_BIT);

    // Make swapchain image optimal for presentation
    image_barrier(frame_cmd, state->swapchain.images[state->swapchain.image_index], VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_NONE,
        VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);

    VK_CHECK(vkEndCommandBuffer(frame_cmd));

    VkSemaphoreSubmitInfo wait_submit = init_semaphore_submit_info(
        state->swapchain.image_acquired[state->swapchain.frame_index],
        VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT);

    VkCommandBufferSubmitInfo cmd_submit = init_command_buffer_submit_info(frame_cmd);
    
    VkSemaphoreSubmitInfo signal_submit = init_semaphore_submit_info(
        state->swapchain.image_present[state->swapchain.frame_index],
        VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT);

    VkSubmitInfo2 submit_info = init_submit_info2(
        1, &wait_submit,
        1, &cmd_submit,
        1, &signal_submit);
    
    result = vkQueueSubmit2(
        state->device.graphics_queue, 
        /* submitCount */ 1,
        &submit_info,
        scene->render_fences[state->swapchain.frame_index]);
    VK_CHECK(result);

    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = 0,
        .swapchainCount = 1,
        .pSwapchains = &state->swapchain.swapchain,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &state->swapchain.image_present[state->swapchain.frame_index],
        .pImageIndices = &state->swapchain.image_index};
    result = vkQueuePresentKHR(state->device.present_queue, &present_info);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        VK_CHECK(vkDeviceWaitIdle(state->device.handle));
        recreate_swapchain(state, &state->swapchain);
    } else VK_CHECK(result);

    state->swapchain.frame_index = (state->swapchain.frame_index + 1) % state->swapchain.image_count;
    return true;
}

// TODO: Have this return the slot in the descriptor array the new combined image sampler is placed
void scene_texture_set(scene* scene, u32 tex_id, u32 img_id, u32 sampler_id) {
    image* img = image_manager_get(scene->image_bank, img_id);
    VkDescriptorImageInfo image_info = {
        .sampler = scene->samplers[sampler_id],
        .imageView = img->view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    VkWriteDescriptorSet img_write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = 0,
        .descriptorCount = 1,
        .dstArrayElement = tex_id,
        .dstBinding = SCENE_SET_TEXTURES_BINDING,
        .dstSet = scene->scene_set,
        .pImageInfo = &image_info,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    };
    vkUpdateDescriptorSets(
        scene->state->device.handle,
        /* WriteCount: */ 1,
        &img_write,
        /* CopyCount: */ 0,
        /* Copies: */ NULL
    );
}
// TODO: END

static b8 scene_on_key_event(u16 code, void* scne, event_data data) {
    scene* s = (scene*)scne;
    keys key = EVENT_DATA_KEY(data);
    switch (key)
    {
    case KEY_L:
        light_offset += 2.0f;
        break;
    case KEY_K:
        light_offset -= 2.0f; 
        break;
    case KEY_J:
        light_dynamic = !light_dynamic;
    }
    return false;
}

void scene_renderer_descriptors_init(scene* scene, renderer_state* state) {
    // Set 0: Scene set layout (engine specific). The set itself will be allocated on the fly
    VkDescriptorBindingFlags ssbf = 
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
        VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT;
    VkDescriptorBindingFlags scene_set_binding_flags[] = {
        [SCENE_SET_FRAME_UNIFORMS_BINDING] = ssbf,
        [SCENE_SET_DRAW_COUNTS_BINDING] = ssbf,
        [SCENE_SET_DRAW_BUFFERS_BINDING] = ssbf,
        [SCENE_SET_OBJECTS_BINDING] = ssbf,
        [SCENE_SET_GEOMETRIES_BINDING] = ssbf,
        [SCENE_SET_VERTICES_BINDING] = ssbf,
        [SCENE_SET_TRANSFORMS_BINDING] = ssbf,
        [SCENE_SET_TEXTURES_BINDING] = ssbf | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT,
    };
    VkDescriptorSetLayoutBindingFlagsCreateInfo scene_binding_flags_create_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .pNext = 0,
        .bindingCount = SCENE_SET_BINDING_MAX,
        .pBindingFlags = scene_set_binding_flags,
    };
    VkDescriptorSetLayoutBinding scene_set_bindings[] = {
        [SCENE_SET_FRAME_UNIFORMS_BINDING] = {
            .binding = SCENE_SET_FRAME_UNIFORMS_BINDING,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = NULL,
        },
        [SCENE_SET_DRAW_COUNTS_BINDING] = {
            .binding = SCENE_SET_DRAW_COUNTS_BINDING,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = NULL,
        },
        [SCENE_SET_DRAW_BUFFERS_BINDING] = {
            .binding = SCENE_SET_DRAW_BUFFERS_BINDING,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = NULL,
        },
        [SCENE_SET_OBJECTS_BINDING] = {
            .binding = SCENE_SET_OBJECTS_BINDING,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = NULL,
        },
        [SCENE_SET_GEOMETRIES_BINDING] = {
            .binding = SCENE_SET_GEOMETRIES_BINDING,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = NULL,
        },
        [SCENE_SET_VERTICES_BINDING] = {
            .binding = SCENE_SET_VERTICES_BINDING,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = NULL,
        },
        [SCENE_SET_TRANSFORMS_BINDING] = {
            .binding = SCENE_SET_TRANSFORMS_BINDING,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = NULL,
        },
        [SCENE_SET_TEXTURES_BINDING] = {
            .binding = SCENE_SET_TEXTURES_BINDING,
            .descriptorCount = state->device.properties_12.maxDescriptorSetUpdateAfterBindSampledImages,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = NULL,
        },
    };
    VkDescriptorSetLayoutCreateInfo scene_layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &scene_binding_flags_create_info,
        .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
        .bindingCount = SCENE_SET_BINDING_MAX,
        .pBindings = scene_set_bindings,
    };
    VK_CHECK(vkCreateDescriptorSetLayout(
        state->device.handle,
        &scene_layout_info,
        state->allocator,
        &scene->scene_set_layout));
    SET_DEBUG_NAME(state, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, scene->scene_set_layout, "Scene Descriptor Set Layout");

    // Set 1: Material layout (material specific). Arrays of each element
    VkDescriptorSetLayoutBinding mat_bindings[] = {
        [MAT_DRAWS_BINDING] = {
            .binding = MAT_DRAWS_BINDING,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = NULL,
        },
        [MAT_INSTANCES_BINDING] = {
            .binding = MAT_INSTANCES_BINDING,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = NULL,
        },
    };
    VkDescriptorBindingFlags mat_binding_flags[] = {
        [MAT_DRAWS_BINDING] = ssbf,
        [MAT_INSTANCES_BINDING] = ssbf,
    };
    VkDescriptorSetLayoutBindingFlagsCreateInfo mat_binding_flags_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .pNext = 0,
        .bindingCount = MAT_BINDING_MAX,
        .pBindingFlags = mat_binding_flags
    };
    VkDescriptorSetLayoutCreateInfo mat_layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &mat_binding_flags_info,
        .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
        .bindingCount = MAT_BINDING_MAX,
        .pBindings = mat_bindings,
    };
    VK_CHECK(vkCreateDescriptorSetLayout(
        state->device.handle,
        &mat_layout_info,
        state->allocator,
        &scene->mat_set_layout));
    SET_DEBUG_NAME(state, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, scene->mat_set_layout, "MaterialDescriptorSetLayout");

    // DescriptorPool
    VkDescriptorPoolSize sizes[] = {
        [0] = {
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = MAX_IMAGE_COUNT,
        },
        [1] = {
            .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 2 * MAT_PIPE_MAX + SCENE_SET_BINDING_MAX,
        },
        [2] = {
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 20,
        },
    };
    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = 0,
        .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
        .maxSets = 1 + MAT_PIPE_MAX,
        .poolSizeCount = 3,
        .pPoolSizes = sizes,
    };
    VK_CHECK(vkCreateDescriptorPool(
        state->device.handle,
        &pool_info,
        state->allocator,
        &scene->descriptor_pool));
    SET_DEBUG_NAME(state, VK_OBJECT_TYPE_DESCRIPTOR_POOL, scene->descriptor_pool, "Scene Descriptor Pool");

    u32 scene_image_count = MAX_IMAGE_COUNT;
    VkDescriptorSetVariableDescriptorCountAllocateInfo scene_descriptor_count_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
        .descriptorSetCount = 1,
        .pDescriptorCounts = &scene_image_count,
    };
    VkDescriptorSetAllocateInfo scene_set_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = &scene_descriptor_count_info,
        .descriptorPool = scene->descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &scene->scene_set_layout,
    };
    VK_CHECK(vkAllocateDescriptorSets(
        state->device.handle,
        &scene_set_alloc_info,
        &scene->scene_set));
    SET_DEBUG_NAME(state, VK_OBJECT_TYPE_DESCRIPTOR_SET, scene->scene_set, "Scene Descriptor Set");

        // Write buffers to DescriptorSet
    VkDescriptorBufferInfo uniform_buffer_info = {
        .buffer = scene->scene_uniforms.handle,
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    };
    VkWriteDescriptorSet uniform_write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = 0,
        .descriptorCount = 1,
        .dstArrayElement = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .dstSet = scene->scene_set,
        .dstBinding = SCENE_SET_FRAME_UNIFORMS_BINDING,
        .pBufferInfo = &uniform_buffer_info,
    };
    VkDescriptorBufferInfo object_buffer_info = {
        .buffer = scene->object_buffer.handle,
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    };
    VkWriteDescriptorSet object_buffer_write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = 0,
        .descriptorCount = 1,
        .dstArrayElement = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .dstSet = scene->scene_set,
        .dstBinding = SCENE_SET_OBJECTS_BINDING,
        .pBufferInfo = &object_buffer_info,
    };
    VkDescriptorBufferInfo draw_count_buffer_info = {
        .buffer = scene->counts_buffer.handle,
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    };
    VkWriteDescriptorSet draw_count_buffer_write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = 0,
        .descriptorCount = 1,
        .dstArrayElement = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .dstSet = scene->scene_set,
        .dstBinding = SCENE_SET_DRAW_COUNTS_BINDING,
        .pBufferInfo = &draw_count_buffer_info,
    };
    VkDescriptorBufferInfo draws_buffer_info = {
        .buffer = scene->draws_buffer.handle,
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    };
    VkWriteDescriptorSet draws_buffer_write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = 0,
        .descriptorCount = 1,
        .dstArrayElement = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .dstSet = scene->scene_set,
        .dstBinding = SCENE_SET_DRAW_BUFFERS_BINDING,
        .pBufferInfo = &draws_buffer_info,
    };
    VkWriteDescriptorSet writes[] = {
        uniform_write,
        object_buffer_write,
        draw_count_buffer_write,
        draws_buffer_write,
    };
    vkUpdateDescriptorSets(
        state->device.handle,
        /* writeCount: */ 4,
        writes,
        /* copyCount: */ 0,
        /* copies: */ 0 
    );
}

void scene_renderer_descriptors_shutdown(scene* scene, renderer_state* state) {
    vkDestroyDescriptorPool(
        state->device.handle,
        scene->descriptor_pool,
        state->allocator);
    vkDestroyDescriptorSetLayout(
        state->device.handle,
        scene->mat_set_layout,
        state->allocator);
    vkDestroyDescriptorSetLayout(
        state->device.handle,
        scene->scene_set_layout,
        state->allocator
    );
}

// TEMP:HACK: Until different material pipelines and shaders are written and supported
static mat_pipe_type payload_material_to_mat_pipe_type(imported_material* material) {
    switch (material->alpha) {
        case ALPHA_MODE_OPAQUE:
        case ALPHA_MODE_MASK:
            return MAT_PIPE_METAL_ROUGH;
        default: {
            return MAT_PIPE_METAL_ROUGH_TRANSPARENT;
        }
    }
}
// TEMP:HACK: END

b8 scene_init_import_payload(scene** scn, renderer_state* state, import_payload* payload) {
    scene* scene = etallocate(sizeof(struct scene), MEMORY_TAG_SCENE);

    // TODO: 1. Create singular vertex buffer, index buffer
    u32 geo_count = dynarray_length(payload->geometries);
    
    vertex* vertices = dynarray_create(0, sizeof(vertex));
    u32* indices = dynarray_create(0, sizeof(u32));
    geometry* geometries = dynarray_create(geo_count, sizeof(geometry));
    dynarray_resize((void**)&geo_count, geo_count);
    for (u32 i = 0; i < geo_count; ++i) {
        geometries[i].vertex_offset = dynarray_length(vertices);
        geometries[i].start_index = dynarray_length(indices);
        geometries[i].index_count = dynarray_length(payload->geometries[i].indices);

        dynarray_append_vertex(&vertices, payload->geometries[i].vertices);
        dynarray_append_u32(&indices, payload->geometries[i].indices);

        dynarray_destroy(payload->geometries[i].vertices);
        dynarray_destroy(payload->geometries[i].indices);
    }
    dynarray_destroy(payload->geometries);

    // TODO: 2. Convert material data to pipeline_id and instance_id/mat_id for the renderer to use
    
    // TODO: 3. Change nodes into objects and transforms for the meshes.
    
    // TODO: 4. Initialize renderer stuff, gpu memory, descriptors, etc...
    
    // TODO: 5. Render and test if working

    // TODO: 6. Will create scene & joint/bone hierarchy eventually. Changes 3

    *scn = scene;
    return false;
}