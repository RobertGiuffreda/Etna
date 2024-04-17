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

#include "resources/importers/importer.h"
#include "resources/image_manager.h"
#include "resources/resource_private.h"
#include "resources/material.h"

#include "renderer/src/vk_types.h"
#include "renderer/src/renderer.h"
#include "renderer/src/utilities/vkinit.h"
#include "renderer/src/utilities/vkutils.h"
#include "renderer/src/image.h"
#include "renderer/src/buffer.h"
#include "renderer/src/shader.h"
#include "renderer/src/pipeline.h"

// TEMP: Until a math library is situated
#include <math.h>
// TEMP: END

// TEMP: Until events refactor
static b8 scene_on_key_event(u16 code, void* scne, event_data data);
// TEMP: END

static b8 scene_renderer_init(scene* scene, scene_config config);
static void scene_renderer_shutdown(scene* scene);

// TODO: Remove, textures will be set when loading for now, until any kind of streaming
// is implemented, if it ever is
void scene_texture_set(scene* scene, u32 tex_id, u32 img_id, u32 sampler_id);

b8 scene_init(scene** scn, scene_config config) {
    scene* scene = etallocate(sizeof(struct scene), MEMORY_TAG_SCENE);
    scene->name = config.name;

    camera_create(&scene->cam);
    scene->cam.position = (v3s){.raw = {0.0f, 0.0f, 5.0f}};

    // NOTE: This will be passed a config when serialization is implemented
    scene->data.ambient_color = (v4s) { .raw = {1.f, 1.f, 1.f, .1f}};
    scene->data.light_color   = (v4s) { .raw = {1.f, 1.f, 1.f, 5.f}};
    scene->data.sun_color     = (v4s) { .raw = {1.f, 1.f, 1.f, 4.f}};
    scene->data.sun_direction = (v4s) { .raw = {-0.707107f, -0.707107f, 0.0f, 0.0f}};
    
    import_payload* payload = config.import_payload;

    // Create singular vertex buffer, index buffer
    vertex* vertices = dynarray_create(0, sizeof(vertex));
    u32* indices = dynarray_create(0, sizeof(u32));
    
    u32 geo_count = dynarray_length(payload->geometries);
    geometry* geometries = dynarray_create(geo_count, sizeof(geometry));
    dynarray_resize((void**)&geometries, geo_count);
    for (u32 i = 0; i < geo_count; ++i) {
        geometries[i] = (geometry) {
            .start_index = dynarray_length(indices),
            .index_count = dynarray_length(payload->geometries[i].indices),
            .vertex_offset = dynarray_length(vertices),
            .radius = payload->geometries[i].radius,
            .origin = payload->geometries[i].origin,
            .extent = payload->geometries[i].extent,
        };

        dynarray_append_vertex(&vertices, payload->geometries[i].vertices);
        dynarray_append_u32(&indices, payload->geometries[i].indices);
    }

    // Remove default pipelines without empty instance arrays
    mat_pipe_config* mat_pipe_configs = dynarray_create(0, sizeof(mat_pipe_config));
    u32 pipeline_count = dynarray_length(payload->pipelines);
    u32* pipe_index_to_id = etallocate(sizeof(u32) * pipeline_count, MEMORY_TAG_SCENE);
    for (u32 i = 0; i < pipeline_count; ++i) {
        u32 instance_count = dynarray_length(payload->pipelines[i].instances);
        if (instance_count) {
            mat_pipe_config config = {
                .vert_path = payload->pipelines[i].vert_path,
                .frag_path = payload->pipelines[i].frag_path,
                .inst_size = payload->pipelines[i].inst_size,
                .inst_count = instance_count,
                .instances = payload->pipelines[i].instances,
                .transparent = payload->pipelines[i].transparent,
            };
            pipe_index_to_id[i] = dynarray_length(mat_pipe_configs);
            dynarray_push((void**)&mat_pipe_configs, &config);
        }
    }
    
    // Change nodes into objects and transforms for the meshes
    m4s* transforms = dynarray_create(1, sizeof(m4s));
    object* objects = dynarray_create(1, sizeof(object));
    u32 node_count = dynarray_length(payload->nodes);
    for (u32 i = 0; i < node_count; ++i) {
        if (payload->nodes[i].has_mesh) {
            u32 transform_index = dynarray_length(transforms);
            dynarray_push((void**)&transforms, &payload->nodes[i].world_transform);

            import_mesh mesh = payload->meshes[payload->nodes[i].mesh_index];
            u64 object_start = dynarray_grow((void**)&objects, mesh.count);
            for (u32 j = 0; j < mesh.count; ++j) {
                u32 material_index = mesh.material_indices[j];
                objects[object_start + j] = (object) {
                    .pipe_id = pipe_index_to_id[payload->mat_index_to_mat_id[material_index].pipe_id],
                    .mat_id = payload->mat_index_to_mat_id[material_index].inst_id,
                    .geo_id = mesh.geometry_indices[j],
                    .transform_id = transform_index,
                };
            }
        }
    }

    scene->vertices = vertices;
    scene->indices = indices;
    scene->objects = objects;
    scene->transforms = transforms;
    scene->geometries = geometries;

    u32 mat_pipe_count = dynarray_length(mat_pipe_configs);
    scene->mat_pipe_count = mat_pipe_count;
    scene->mat_pipe_configs = mat_pipe_configs;
    etfree(pipe_index_to_id, sizeof(u32) * pipeline_count, MEMORY_TAG_SCENE);

    scene->payload = *payload;
    event_observer_register(EVENT_CODE_KEY_RELEASE, scene, scene_on_key_event);
    *scn = scene;


    if (!scene_renderer_init(scene, config)) {
        ETFATAL("Scene renderer failed to initialize.");
        return false;
    }
    return true;
}

void scene_shutdown(scene* scene) {
    renderer_state* state = scene->state;

    event_observer_deregister(EVENT_CODE_KEY_RELEASE, scene, scene_on_key_event);

    scene_renderer_shutdown(scene);
    camera_destroy(&scene->cam);

    dynarray_destroy(scene->indices);
    dynarray_destroy(scene->vertices);
    dynarray_destroy(scene->geometries);
    dynarray_destroy(scene->transforms);
    dynarray_destroy(scene->objects);

    import_payload_destroy(&scene->payload);
    
    etfree(scene, sizeof(struct scene), MEMORY_TAG_SCENE);
}

// TEMP: For testing and debugging
static f32 light_offset = 0.0f;
static b8 light_dynamic = true;
// TEMP: END

void scene_update(scene* scene, f64 dt) {
    renderer_state* state = scene->state;

    camera_update(&scene->cam, dt);

    // TODO: Camera should store near and far values & calculate perspective matrix
    // TODO: Scene should register for event system and update camera stuff itself
    m4s view = camera_get_view_matrix(&scene->cam);
    m4s project = glms_perspective(
        /* fovy */ glm_rad(70.f),
        ((f32)state->swapchain.image_extent.width/(f32)state->swapchain.image_extent.height), 
        /* near-z: */ 10000.f,
        /* far-z: */ 0.1f);
    // NOTE: invert the Y direction on projection matrix so that we are more match gltf axis
    project.raw[1][1] *= -1;
    // NOTE: END
    // TODO: END
    // TODO: END

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

b8 scene_renderer_init(scene* scene, scene_config config) {
    renderer_state* state = config.renderer_state;
    scene->state = state;

    scene->data.max_draw_count = MAX_DRAW_COMMANDS; // TODO: Better method??

    // Rendering resolution
    scene->render_extent = (VkExtent3D) {
        .width = config.resolution_width * 2,   // TEMP: Super Sample Anti Aliasing
        .height = config.resolution_height * 2, // TEMP: Super Sample Anti Aliasing
        .depth = 1,
    };

    // Color attachment
    VkImageUsageFlags draw_image_usages = 0;
    draw_image_usages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    draw_image_usages |= VK_IMAGE_USAGE_STORAGE_BIT;
    draw_image_usages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    image2D_create(state,
        scene->render_extent,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        draw_image_usages,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &scene->render_image);
    SET_DEBUG_NAME(state, VK_OBJECT_TYPE_IMAGE, scene->render_image.handle, "MainRenderImage");

    // Depth attachment
    VkImageUsageFlags depth_image_usages = {0};
    depth_image_usages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    image2D_create(state, 
        scene->render_extent,
        VK_FORMAT_D32_SFLOAT,
        depth_image_usages,
        VK_IMAGE_ASPECT_DEPTH_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &scene->depth_image);
    SET_DEBUG_NAME(state, VK_OBJECT_TYPE_IMAGE, scene->depth_image.handle, "MainDepthImage");

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
        DEBUG_BLOCK(
            char fence_name[] = "RenderFence X";
            fence_name[str_length(fence_name) - 1] = '0' + i;
            char cmd_pool_name[] = "GraphicsCommandPool X";
            cmd_pool_name[str_length(cmd_pool_name) - 1] = '0' + i;
            char cmd_buff_name[] = "GraphicsCommandBuffer X";
            cmd_buff_name[str_length(cmd_buff_name) - 1] = '0' + i;
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

    // Buffers
    buffer_create(
        state,
        sizeof(scene_data),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &scene->scene_uniforms);
    SET_DEBUG_NAME(state, VK_OBJECT_TYPE_BUFFER, scene->scene_uniforms.handle, "SceneFrameUniformBuffer");
    buffer_create(
        state,
        sizeof(u32) * scene->mat_pipe_count,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &scene->counts_buffer);
    SET_DEBUG_NAME(state, VK_OBJECT_TYPE_BUFFER, scene->counts_buffer.handle, "PipelineDrawCountsBuffer");
    buffer_create(
        state,
        sizeof(VkDeviceAddress) * scene->mat_pipe_count,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &scene->draws_buffer);
    SET_DEBUG_NAME(state, VK_OBJECT_TYPE_BUFFER, scene->draws_buffer.handle, "PipelineDrawBufferPointersBuffer");

    u64 vertex_count = dynarray_length(scene->vertices);
    u64 index_count = dynarray_length(scene->indices);
    mesh_buffers vertex_index = upload_mesh_immediate(
        state,
        index_count,
        scene->indices,
        vertex_count,
        scene->vertices);
    scene->vertex_buffer = vertex_index.vertex_buffer;
    scene->index_buffer = vertex_index.index_buffer;
    SET_DEBUG_NAME(state, VK_OBJECT_TYPE_BUFFER, scene->vertex_buffer.handle, "SceneVertexBuffer");
    SET_DEBUG_NAME(state, VK_OBJECT_TYPE_BUFFER, scene->index_buffer.handle, "SceneIndexBuffer");

    buffer_create_data(
        state,
        scene->objects,
        sizeof(object) * dynarray_length(scene->objects),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &scene->object_buffer);
    SET_DEBUG_NAME(state, VK_OBJECT_TYPE_BUFFER, scene->object_buffer.handle, "ObjectBuffer");

    buffer_create_data(
        state,
        scene->transforms,
        sizeof(m4s) * dynarray_length(scene->transforms),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &scene->transform_buffer);
    SET_DEBUG_NAME(state, VK_OBJECT_TYPE_BUFFER, scene->transform_buffer.handle, "TransformBuffer");
    
    buffer_create_data(
        state,
        scene->geometries,
        sizeof(geometry) * dynarray_length(scene->geometries),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &scene->geometry_buffer);
    SET_DEBUG_NAME(state, VK_OBJECT_TYPE_BUFFER, scene->geometry_buffer.handle, "GeometryBuffer");

    // NOTE: Descriptors init function placed here for testing payload with x amount of mat_pipe_configs
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
            .descriptorCount = MAX_TEXTURE_COUNT,
        },
        [1] = {
            .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 2 * scene->mat_pipe_count + SCENE_SET_BINDING_MAX,
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
        .maxSets = 1 + scene->mat_pipe_count,
        .poolSizeCount = 3,
        .pPoolSizes = sizes,
    };
    VK_CHECK(vkCreateDescriptorPool(
        state->device.handle,
        &pool_info,
        state->allocator,
        &scene->descriptor_pool));
    SET_DEBUG_NAME(state, VK_OBJECT_TYPE_DESCRIPTOR_POOL, scene->descriptor_pool, "Scene Descriptor Pool");

    u32 scene_texture_count = MAX_TEXTURE_COUNT;
    VkDescriptorSetVariableDescriptorCountAllocateInfo scene_descriptor_count_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
        .descriptorSetCount = 1,
        .pDescriptorCounts = &scene_texture_count,
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
    VkWriteDescriptorSet uniform_buffer_write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = 0,
        .descriptorCount = 1,
        .dstArrayElement = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .dstSet = scene->scene_set,
        .dstBinding = SCENE_SET_FRAME_UNIFORMS_BINDING,
        .pBufferInfo = &uniform_buffer_info,
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
    VkDescriptorBufferInfo geometry_buffer_info = {
        .buffer = scene->geometry_buffer.handle,
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    };
    VkWriteDescriptorSet geometry_buffer_write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = 0,
        .descriptorCount = 1,
        .dstArrayElement = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .dstSet = scene->scene_set,
        .dstBinding = SCENE_SET_GEOMETRIES_BINDING,
        .pBufferInfo = &geometry_buffer_info,
    };
    VkDescriptorBufferInfo vertex_buffer_info = {
        .buffer = scene->vertex_buffer.handle,
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    };
    VkWriteDescriptorSet vertex_buffer_write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = 0,
        .descriptorCount = 1,
        .dstArrayElement = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .dstSet = scene->scene_set,
        .dstBinding = SCENE_SET_VERTICES_BINDING,
        .pBufferInfo = &vertex_buffer_info,
    };
    VkDescriptorBufferInfo transform_buffer_info = {
        .buffer = scene->transform_buffer.handle,
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    };
    VkWriteDescriptorSet transform_buffer_write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = 0,
        .descriptorCount = 1,
        .dstArrayElement = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .dstSet = scene->scene_set,
        .dstBinding = SCENE_SET_TRANSFORMS_BINDING,
        .pBufferInfo = &transform_buffer_info,
    };
    VkWriteDescriptorSet buffer_writes[] = {
        uniform_buffer_write,
        object_buffer_write,
        draw_count_buffer_write,
        draws_buffer_write,
        geometry_buffer_write,
        vertex_buffer_write,
        transform_buffer_write,
    };
    vkUpdateDescriptorSets(
        state->device.handle,
        /* writeCount: */ 7,
        buffer_writes,
        /* copyCount: */ 0,
        /* copies: */ 0
    );
    // NOTE: Descriptors init: END

    // Images
    image_manager_initialize(&scene->image_bank, state);
    u32 image_count = dynarray_length(scene->payload.images);
    for (u32 i = 0; i < image_count; ++i) {
        import_image image = scene->payload.images[i];
        if (image.data) {
            image2D_config image_config = {
                .name = image.name,
                .height = image.height,
                .width = image.width,
                .data = image.data,
            };
            image2D_submit(scene->image_bank, &image_config);
        } else {
            image_manager_increment(scene->image_bank);
        }
    }

    // Samplers
    scene->sampler_count = dynarray_length(scene->payload.samplers);
    scene->samplers = etallocate(sizeof(VkSampler) * scene->sampler_count, MEMORY_TAG_SCENE);
    for (u32 i = 0; i < scene->sampler_count; ++i) {
        VkSamplerCreateInfo sampler_info = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .maxLod = VK_LOD_CLAMP_NONE,
            .minLod = 0,
            .magFilter = scene->payload.samplers[i].mag_filter,
            .minFilter = scene->payload.samplers[i].min_filter,
            .mipmapMode = scene->payload.samplers[i].mip_map_mode,
        };
        VK_CHECK(vkCreateSampler(
            state->device.handle,
            &sampler_info,
            state->allocator,
            &scene->samplers[i]
        ));
    }

    // Texture defaults using default samplers and images from renderer_state
    VkDescriptorImageInfo white_image_info = {
        .sampler = state->linear_smpl,
        .imageView = state->default_white.view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    VkWriteDescriptorSet white_image_write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = 0,
        .descriptorCount = 1,
        .dstArrayElement = DEFAULT_TEXTURE_WHITE,
        .dstBinding = SCENE_SET_TEXTURES_BINDING,
        .dstSet = scene->scene_set,
        .pImageInfo = &white_image_info,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    };
    VkDescriptorImageInfo black_image_info = {
        .sampler = state->linear_smpl,
        .imageView = state->default_black.view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    VkWriteDescriptorSet black_image_write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = 0,
        .descriptorCount = 1,
        .dstArrayElement = DEFAULT_TEXTURE_BLACK,
        .dstBinding = SCENE_SET_TEXTURES_BINDING,
        .dstSet = scene->scene_set,
        .pImageInfo = &black_image_info,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    };
    VkDescriptorImageInfo normal_image_info = {
        .sampler = state->linear_smpl,
        .imageView = state->default_normal.view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    VkWriteDescriptorSet normal_image_write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = 0,
        .descriptorCount = 1,
        .dstArrayElement = DEFAULT_TEXTURE_NORMAL,
        .dstBinding = SCENE_SET_TEXTURES_BINDING,
        .dstSet = scene->scene_set,
        .pImageInfo = &normal_image_info,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    };
    VkWriteDescriptorSet default_image_writes[] = {
        white_image_write,
        black_image_write,
        normal_image_write,
    };
    vkUpdateDescriptorSets(
        state->device.handle,
        DEFAULT_TEXTURE_COUNT,
        default_image_writes,
        /* copy count */ 0,
        /* copies */ NULL
    );

    // Textures
    u32 texture_count = dynarray_length(scene->payload.textures);
    for (u32 i = DEFAULT_TEXTURE_COUNT; i < texture_count; ++i) {
        scene_texture_set(scene, i, scene->payload.textures[i].image_id, scene->payload.textures[i].sampler_id);
    }

    const char* draw_gen_path = "assets/shaders/draws.comp.spv.opt";
    shader draw_gen;
    if (!load_shader(state, draw_gen_path, &draw_gen)) {
        ETFATAL("Unable to load draw generation shader.");
        return false;
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
    scene->mat_pipes =  etallocate(sizeof(mat_pipe) * scene->mat_pipe_count, MEMORY_TAG_SCENE);
    for (u32 i = 0; i < scene->mat_pipe_count; ++i) {
        mat_pipe_init(&scene->mat_pipes[i], scene, state, &scene->mat_pipe_configs[i]);
        VkDeviceAddress mat_draws_addr = buffer_get_address(state, &scene->mat_pipes[i].draws_buffer);
        etcopy_memory((VkDeviceAddress*)draw_buffer_addresses + i, &mat_draws_addr, sizeof(VkDeviceAddress));
    }
    vkUnmapMemory(state->device.handle, scene->draws_buffer.memory);
    return true;
}

void scene_renderer_shutdown(scene* scene) {
    renderer_state* state = scene->state;

    // NOTE: This is needed to make sure objects are not destroyed while in use
    vkDeviceWaitIdle(state->device.handle);

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
        state->allocator);

    // TODO: Update to some kind of texture/image management
    image_manager_shutdown(scene->image_bank);
    // TODO: END

    for (u32 i = 0; i < scene->sampler_count; ++i)
        vkDestroySampler(state->device.handle, scene->samplers[i], state->allocator);
    etfree(scene->samplers, sizeof(VkSampler) * scene->sampler_count, MEMORY_TAG_SCENE);

    for (u32 i = 0; i < scene->mat_pipe_count; ++i)
        mat_pipe_shutdown(&scene->mat_pipes[i], scene, state);
    dynarray_destroy(scene->mat_pipe_configs);
    etfree(scene->mat_pipes, sizeof(mat_pipe) * scene->mat_pipe_count, MEMORY_TAG_SCENE);

    u32 frame_overlap = state->swapchain.image_count;
    for (u32 i = 0; i < frame_overlap; ++i) {
        vkDestroyCommandPool(state->device.handle, scene->graphics_pools[i], state->allocator);
        vkDestroyFence(state->device.handle, scene->render_fences[i], state->allocator);
    }
    etfree(scene->graphics_command_buffers, sizeof(VkCommandBuffer) * frame_overlap, MEMORY_TAG_SCENE);
    etfree(scene->graphics_pools, sizeof(VkCommandPool) * frame_overlap, MEMORY_TAG_SCENE);
    etfree(scene->render_fences, sizeof(VkFence) * frame_overlap, MEMORY_TAG_SCENE);

    image_destroy(state, &scene->depth_image);
    image_destroy(state, &scene->render_image);
}

// TODO: Move this function to scene_renderer_begin??
b8 scene_render(scene* scene, renderer_state* state) {
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
        sizeof(u32) * scene->mat_pipe_count,
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
    for (u32 i = 0; i < scene->mat_pipe_count; ++i) {
        buffer_barrier(
            cmd, scene->mat_pipes[i].draws_buffer.handle, /* Offset */ 0, VK_WHOLE_SIZE,
            VK_ACCESS_2_SHADER_WRITE_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT
        );
    }
    buffer_barrier(
        cmd, scene->counts_buffer.handle, /* offset: */ 0, VK_WHOLE_SIZE,
        VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT
    );
    // NOTE: END

    VkClearValue clear_color = {
        .color = {0.f,0.f,0.f,0.f},
    };
    VkRenderingAttachmentInfo color_attachment = init_color_attachment_info(
        scene->render_image.view, &clear_color, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo depth_attachment = init_depth_attachment_info(
        scene->depth_image.view, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    VkExtent2D render_extent = {.width = scene->render_extent.width, .height = scene->render_extent.height};
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

    for (u32 i = 0; i < scene->mat_pipe_count; ++i) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, scene->mat_pipes[i].pipe);

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, scene->mat_pipeline_layout, 1, 1, &scene->mat_pipes[i].set, 0, NULL);
        
        vkCmdDrawIndexedIndirectCount(cmd,
            scene->mat_pipes[i].draws_buffer.handle,
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

    image_barrier(frame_cmd, scene->render_image.handle, scene->render_image.aspects,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_ACCESS_2_NONE, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
        VK_PIPELINE_STAGE_2_NONE, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

    image_barrier(frame_cmd, scene->depth_image.handle, scene->depth_image.aspects,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        VK_ACCESS_2_NONE, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT);

    draw_geometry(state, scene, frame_cmd);

    // Make render image optimal layout for transfer source to swapchain image
    image_barrier(frame_cmd, scene->render_image.handle, scene->render_image.aspects,
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
        scene->render_image.handle,
        state->swapchain.images[state->swapchain.image_index],
        scene->render_extent,
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