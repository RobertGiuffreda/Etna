#include "scene.h"
#include "scene/scene_private.h"

#include "utils/dynarray.h"

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
static void scene_renderer_shutdown(scene* scene, renderer_state* state);

// TODO: Remove, textures will be set when loading for now, 
// until any kind of streaming is implemented, if it ever is.
void scene_texture_set(scene* scene, u32 tex_id, u32 img_id, u32 sampler_id);
// TODO: END

void add_node_to_transforms(u32 node_index, u32* nodes_to_transforms, import_node* nodes, transforms transforms);

b8 scene_init(scene** scn, scene_config config) {
    scene* scene = etallocate(sizeof(struct scene), MEMORY_TAG_SCENE);
    scene->name = config.name;

    camera_create(&scene->cam);
    scene->cam.position = (v3s){.raw = {0.0f, 0.0f, 1.0f}};

    // NOTE: This will be passed a config when serialization is implemented
    scene->data.ambient_color = (v4s) { .raw = {1.f, 1.f, 1.f, .1f}};
    scene->data.light.color   = (v4s) { .raw = {1.f, 1.f, 1.f, 5.f}};
    scene->data.sun.color     = (v4s) { .raw = {1.f, 1.f, 1.f, 10.f}};
    scene->data.sun.direction = (v4s) { .raw = {0.00001f, -1.f, 0.00001f, 0.0f}};
    // scene->data.sun.direction = (v4s) { .raw = {-0.707107f, -0.707107f, 0.0f, 0.0f}};

    scene->data.debug_view = DEBUG_VIEW_TYPE_OFF;
    
    import_payload* payload = config.import_payload;

    // Create singular vertex buffer, index buffer
    vertex* vertices = payload->vertices.statics;
    skin_vertex* skin_vertices = payload->vertices.skinned;
    u32* indices = payload->indices;
    
    u32 geo_count = dynarray_length(payload->geometries);
    geometry* geometries = dynarray_create(geo_count, sizeof(geometry));
    dynarray_resize((void**)&geometries, geo_count);
    for (u32 i = 0; i < geo_count; ++i) {
        geometries[i] = (geometry) {
            .start_index = payload->geometries[i].index_start,
            .index_count = payload->geometries[i].index_count,
            .radius = payload->geometries[i].radius,
            .origin = payload->geometries[i].origin,
            .extent = payload->geometries[i].extent,
        };
    }

    // NOTE: Remove default pipelines with empty instance arrays
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

    // NOTE: Reorders node hierarchy into flat transform array 
    u32 node_count = dynarray_length(payload->nodes);
    u32* nodes_to_transforms = etallocate(sizeof(u32) * node_count, MEMORY_TAG_SCENE);

    transforms transforms = transforms_init(node_count);

    u32 root_node_count = dynarray_length(payload->root_nodes);
    for (u32 i = 0; i < root_node_count; ++i) {
        u32 root_index = payload->root_nodes[i];
        import_node root = payload->nodes[root_index];
        transform root_transform = matrix_to_transform(root.local_transform);
        nodes_to_transforms[root_index] = transforms_add_root(transforms, root_transform);
        
        u32 children_count = dynarray_length(root.children_indices);
        for (u32 j = 0; j < children_count; ++j) {
            add_node_to_transforms(root.children_indices[j], nodes_to_transforms, payload->nodes, transforms);
        }
    }

    transforms_recompute_global(transforms);

    // NOTE: Change index into old node array into index into
    // the new transform array
    u32 skin_count = dynarray_length(payload->skins);
    for (u32 i = 0; i < skin_count; ++i) {
        u32 joint_count = dynarray_length(payload->skins[i].joint_indices);
        for (u32 j = 0; j < joint_count; ++j) {
            u32 joint_index = payload->skins[i].joint_indices[j];
            payload->skins[i].joint_indices[j] = nodes_to_transforms[joint_index];
        }
    }

    u32 animation_count = dynarray_length(payload->animations);
    for (u32 i = 0; i < animation_count; ++i) {
        u32 channel_count = dynarray_length(payload->animations[i].channels);
        for (u32 j = 0; j < channel_count; ++j) {
            u32 target_index = payload->animations[i].channels[j].target_index;
            if (payload->animations[i].channels[j].trans_type == TRANSFORM_TYPE_WEIGHTS) {
                // TODO: Have this be the mesh instance index
                payload->animations[i].channels[j].target_index = payload->nodes[target_index].mesh_index;
                // TODO: END
            } else {
                payload->animations[i].channels[j].target_index = nodes_to_transforms[target_index];
            }
        }
    }

    object* objects = dynarray_create(1, sizeof(object));
    for (u32 i = 0; i < node_count; ++i) {
        if (payload->nodes[i].has_mesh) {
            u32 transform_index = nodes_to_transforms[i];
            if (payload->nodes[i].has_skin) {
                continue;
            }
            import_mesh mesh = payload->meshes[payload->nodes[i].mesh_index];
            u64 object_start = dynarray_grow((void**)&objects, mesh.geo_count);
            for (u32 j = 0; j < mesh.geo_count; ++j) {
                mat_id material_index = mesh.materials[j];
                objects[object_start + j] = (object) {
                    .pipe_id = pipe_index_to_id[material_index.pipe_id],
                    .mat_id = material_index.inst_id,
                    .geo_id = mesh.geometries[j],
                    .vertex_offset = mesh.vertex_offset,
                    .transform_id = transform_index,
                };
            }
        }
    }

    u32 extra_vertex_offset = dynarray_length(vertices);
    u32 extra_vertex_count = 0;

    u32 skin_instance_count = dynarray_length(payload->skin_instances);
    skin_instance* skin_instances = dynarray_create(skin_instance_count, sizeof(skin_instance));
    dynarray_resize((void**)&skin_instances, skin_instance_count);
    for (u32 i = 0; i < skin_instance_count; ++i) {
        skin_instances[i].skin = payload->skin_instances[i].skin;
        u32 mesh_inst_count = dynarray_length(payload->skin_instances[i].meshes);
        skin_instances[i].meshes = dynarray_create(mesh_inst_count, sizeof(skin_mesh_inst));
        dynarray_resize((void**)&skin_instances[i].meshes, mesh_inst_count);
        for (u32 j = 0; j < mesh_inst_count; ++j) {
            u32 mesh_index = payload->skin_instances[i].meshes[j];
            skin_instances[i].meshes[j].mesh = mesh_index;
            skin_instances[i].meshes[j].vertex_offset = extra_vertex_offset;

            import_mesh mesh = payload->meshes[mesh_index];
            u64 object_start = dynarray_grow((void**)&objects, mesh.geo_count);
            for (u32 k = 0; k < mesh.geo_count; ++k) {
                mat_id material_index = mesh.materials[k];
                objects[object_start + k] = (object) {
                    .pipe_id = pipe_index_to_id[material_index.pipe_id],
                    .mat_id = material_index.inst_id,
                    .geo_id = mesh.geometries[k],
                    .vertex_offset = extra_vertex_offset,
                    // TEMP:HACK: 0 index: Dummy transform identity mat in transform struct
                    // to keep the gltf skins using node hierarchy logic intact
                    .transform_id = 0,
                    // TEMP:HACK: END
                };
            }

            extra_vertex_offset += mesh.vertex_count;
            extra_vertex_count += mesh.vertex_count;
        }
    }

    etfree(nodes_to_transforms, sizeof(u32) * node_count, MEMORY_TAG_SCENE);

    // TEMP: Testing animations updating transforms on the gpu
    scene->current_animation = 0;
    scene->timestamp = 0.f;
    // TEMP: END

    scene->vertices = vertices;
    scene->skin_vertices = skin_vertices;
    scene->indices = indices;
    scene->objects = objects;
    scene->geometries = geometries;
    scene->transforms = transforms;
    scene->skins = skin_instances;
    scene->extra_vertex_count = extra_vertex_count;

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

    scene_renderer_shutdown(scene, state);

    camera_destroy(&scene->cam);

    dynarray_destroy(scene->indices);
    dynarray_destroy(scene->vertices);
    dynarray_destroy(scene->geometries);
    dynarray_destroy(scene->objects);

    transforms_deinit(scene->transforms);

    import_payload_destroy(&scene->payload);
    
    etfree(scene, sizeof(struct scene), MEMORY_TAG_SCENE);
}

// TEMP: Until my own concept of animations and keyframes is
// ironed out and imported from GLTF files I'm using theirs.
void apply_animation(transforms tforms, import_animation animation, f32 timestamp) {
    u32 channel_count = dynarray_length(animation.channels);
    for (u32 i = 0; i < channel_count; ++i) {
        u32 smpl_index = animation.channels[i].sampler_index;
        u32 target_index = animation.channels[i].target_index;

        u32 key0 = -1;
        u32 key1 = -1;
        f32* timestamps = animation.samplers[smpl_index].timestamps;
        v4s* data = animation.samplers[smpl_index].data;
        u32 key_count = dynarray_length(timestamps);

        if (timestamp > timestamps[key_count - 1]) {
            continue;
        }
        for (u32 j = 0; j < key_count - 1; ++j) {
            if (timestamp >= timestamps[j] && timestamp < timestamps[j + 1]) {
                key0 = j;
                key1 = j + 1;
                break;
            }
        }

        if (key0 == (u32)-1 && key1 == (u32)-1 && timestamp < timestamps[0]) {
            switch (animation.channels[i].trans_type) {
                case TRANSFORM_TYPE_TRANSLATION:
                    transforms_set_translation(tforms, target_index, glms_vec3(data[0]));
                    break;
                case TRANSFORM_TYPE_ROTATION:
                    quat rot = {
                        .x = data[0].x,
                        .y = data[0].y,
                        .z = data[0].z,
                        .w = data[0].w,
                    };
                    transforms_set_rotation(tforms, target_index, rot);
                    break;
                case TRANSFORM_TYPE_SCALE:
                    transforms_set_scale(tforms, target_index, glms_vec3(data[0]));
                    break;
                default: {
                    break;
                }
            }
            continue;
        }

        f32 t0 = timestamps[key0];
        f32 t1 = timestamps[key1];
        v4s data0 = data[key0];
        v4s data1 = data[key1];

        // Only support linear for testing
        ETASSERT(animation.samplers[smpl_index].interp_type == INTERPOLATION_TYPE_LINEAR);

        f32 a = (timestamp - t0) / (t1 - t0);
        switch (animation.channels[i].trans_type) {
            case TRANSFORM_TYPE_TRANSLATION:
                v4s t = glms_vec4_lerp(data0, data1, a);
                transforms_set_translation(tforms, target_index, glms_vec3(t));
                break;
            case TRANSFORM_TYPE_ROTATION:
                quat r;
                glm_quat_slerp(data0.raw, data1.raw, a, r.raw);
                transforms_set_rotation(tforms, target_index, r);
                break;
            case TRANSFORM_TYPE_SCALE:
                v4s s = glms_vec4_lerp(data0, data1, a);
                transforms_set_scale(tforms, target_index, glms_vec3(s));
                break;
            case TRANSFORM_TYPE_WEIGHTS:
                
                break;
            default: {
                ETERROR("Animation of unknown transform type");
                break;
            }
        }
    }
}
// TEMP: END

// TEMP: For testing and debugging
static f32 light_offset = 0.0f;
static b8 light_dynamic = true;
static b8 sun_pov = false;
static b8 sun_pov_persp = false;
// TEMP: END

void scene_update(scene* scene, f64 dt) {
    renderer_state* state = scene->state;
    camera_update(&scene->cam, dt);

    // TODO: Camera should store near and far values & calculate perspective matrix
    // TODO: Camera should have the aspect ratio and have resize function
    // TODO: Scene should register for event system and call camera update functions
    m4s view = camera_get_view_matrix(&scene->cam);
    // NOTE: invert the Y direction on projection matrix so that we match gltf axis
    float aspect_ratio = ((f32)state->swapchain.image_extent.width/(f32)state->swapchain.image_extent.height);
    m4s project = glms_perspective(glm_rad(70.f), aspect_ratio, 1000.f, 0.1f);
    project.raw[1][1] *= -1;
    // NOTE: END
    // TODO: END
    // TODO: END
    // TODO: END

    scene->data.view = view;
    scene->data.proj = project;
    scene->data.viewproj = glms_mat4_mul(project, view);
    scene->data.view_pos = glms_vec4(scene->cam.position, 1.0f);

    m4s sun_view = glms_look(
        (v3s){ .raw = {0.0f, 0.0f, 0.0f}},
        glms_vec3(scene->data.sun.direction),
        (v3s){ .raw = {0.0f, 1.0f, 0.0f}}
    );
    
    // NOTE: invert the Y direction on projection matrix so that we match gltf axis
    m4s sun_projection = glms_ortho(-20.f, 20.f, -20.f, 20.f, 25.f, -25.f);
    sun_projection.raw[1][1] *= -1;

    m4s sun_viewproj = glms_mat4_mul(sun_projection, sun_view);
    scene->data.sun_viewproj = sun_viewproj;

    if (sun_pov) {
        if (sun_pov_persp) sun_viewproj = glms_mat4_mul(project, sun_view);
        scene->data.viewproj = sun_viewproj;
    }

    // Update light information
    v4s l_pos = glms_vec4(scene->cam.position, 1.0f);
    if (light_dynamic) {
        scene->data.light.position = l_pos;
        scene->data.light.position.y += light_offset;
    }

    // TEMP: Apply temporary representation of animation to check if gpu systems are working
    if (dynarray_length(scene->payload.animations)) {
        import_animation curr_ani = scene->payload.animations[scene->current_animation];
        scene->timestamp = fmod(scene->timestamp + dt, curr_ani.duration);
        apply_animation(scene->transforms, curr_ani, scene->timestamp);
        // transforms_recompute_global(scene->transforms);
    }

    // TEMP: Apply inverse binds, until we do not use transforms like gltf does nodes
    transforms_recompute_global(scene->transforms);
    u32 skin_inst_count = dynarray_length(scene->skins);
    for (u32 i = 0; i < skin_inst_count; ++i) {
        import_skin skin = scene->payload.skins[scene->skins[i].skin];
        u32 joint_count = dynarray_length(skin.joint_indices);
        for (u32 j = 0; j < joint_count; j++) {
            u32 transform_index = skin.joint_indices[j];
            scene->transforms.global[transform_index] = glms_mat4_mul(
                scene->transforms.global[transform_index],
                skin.inverse_binds[j]
            );
        }
    }

    // TEMP: END
}

b8 scene_renderer_init(scene* scene, scene_config config) {
    renderer_state* state = config.renderer_state;
    scene->state = state;

    // Rendering resolution
    scene->render_extent = (VkExtent3D) {
        .width = config.resolution_width,
        .height = config.resolution_height,
        .depth = 1,
    };

    // TEMP: SSAA, Super Sample Anti Aliasing
    // scene->render_extent.width *= 2;
    // scene->render_extent.height *= 2;

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
    SET_DEBUG_NAME(state, VK_OBJECT_TYPE_BUFFER, scene->scene_uniforms.handle, "FrameUniformsBuffer");
    buffer_create(
        state,
        sizeof(u32) * (scene->mat_pipe_count + /* TEMP: Shadow map draw commands */ 1),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &scene->counts_buffer);
    SET_DEBUG_NAME(state, VK_OBJECT_TYPE_BUFFER, scene->counts_buffer.handle, "PipelineDrawCountsBuffer");
    buffer_create(
        state,
        sizeof(VkDeviceAddress) * (scene->mat_pipe_count + /* TEMP: Shadow map draw commands */ 1),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &scene->draws_buffer);
    SET_DEBUG_NAME(state, VK_OBJECT_TYPE_BUFFER, scene->draws_buffer.handle, "PipelineDrawBufferPointersBuffer");

    u64 vertex_count = dynarray_length(scene->vertices);
    u64 index_count = dynarray_length(scene->indices);
    vertex_index_buffers vertex_index = vertex_index_buffers_create(
        state,
        index_count,
        scene->indices,
        vertex_count,
        scene->vertices,
        scene->extra_vertex_count
    );
    scene->vertex_buffer = vertex_index.vertex_buffer;
    scene->index_buffer = vertex_index.index_buffer;
    SET_DEBUG_NAME(state, VK_OBJECT_TYPE_BUFFER, scene->vertex_buffer.handle, "VertexBuffer");
    SET_DEBUG_NAME(state, VK_OBJECT_TYPE_BUFFER, scene->index_buffer.handle, "IndexBuffer");

    buffer_create_data(
        state,
        scene->skin_vertices,
        sizeof(skin_vertex) * dynarray_length(scene->skin_vertices),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &scene->skin_vertex_buffer);
    SET_DEBUG_NAME(state, VK_OBJECT_TYPE_BUFFER, scene->skin_vertex_buffer.handle, "SkinVertexBuffer");

    buffer_create_data(
        state,
        scene->objects,
        sizeof(object) * dynarray_length(scene->objects),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &scene->object_buffer);
    SET_DEBUG_NAME(state, VK_OBJECT_TYPE_BUFFER, scene->object_buffer.handle, "ObjectBuffer");

    u64 transform_buffer_bytes = sizeof(m4s) * scene->transforms.capacity;
    buffer_create_data(
        state,
        scene->transforms.global,
        transform_buffer_bytes,
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

    // TEMP: Create some sort of staging system to request memory to send data to GPU instead of this
    u64 staging_size = transform_buffer_bytes + sizeof(scene_data);
    scene->staging_buffers = etallocate(sizeof(buffer) * state->swapchain.image_count, MEMORY_TAG_SCENE);
    scene->staging_mapped = etallocate(sizeof(u8*) * state->swapchain.image_count, MEMORY_TAG_SCENE);
    for (u32 i = 0; i < state->swapchain.image_count; ++i) {
        buffer_create(
            state,
            staging_size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
            &scene->staging_buffers[i]);
        SET_DEBUG_NAME(state, VK_OBJECT_TYPE_BUFFER, scene->staging_buffers[i].handle, "SceneStagingBuffer");
        vkMapMemory(
            state->device.handle,
            scene->staging_buffers[i].memory,
            /* Offset */ 0,
            VK_WHOLE_SIZE,
            /* flags */ 0,
            &scene->staging_mapped[i]
        );
    }
    // TEMP: END

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
        [SCENE_SET_SKIN_VERTICES_BINDING] = ssbf,
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
        [SCENE_SET_SKIN_VERTICES_BINDING] = {
            .binding = SCENE_SET_SKIN_VERTICES_BINDING,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
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
            .descriptorCount = MAX_TEXTURE_COUNT,
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
        .pBindingFlags = mat_binding_flags,
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
    SET_DEBUG_NAME(state, VK_OBJECT_TYPE_DESCRIPTOR_POOL, scene->descriptor_pool, "SceneDescriptorPool");

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
    SET_DEBUG_NAME(state, VK_OBJECT_TYPE_DESCRIPTOR_SET, scene->scene_set, "SceneDescriptorSet");

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
    VkDescriptorBufferInfo skin_vertex_buffer_info = {
        .buffer = scene->skin_vertex_buffer.handle,
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    };
    VkWriteDescriptorSet skin_vertex_buffer_write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = 0,
        .descriptorCount = 1,
        .dstArrayElement = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .dstSet = scene->scene_set,
        .dstBinding = SCENE_SET_SKIN_VERTICES_BINDING,
        .pBufferInfo = &skin_vertex_buffer_info,
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
        skin_vertex_buffer_write,
        transform_buffer_write,
    };
    vkUpdateDescriptorSets(
        state->device.handle,
        /* writeCount: */ 8,
        buffer_writes,
        /* copyCount: */ 0,
        /* copies: */ 0
    );
    // NOTE: Descriptors init: END

    // Images
    image_manager_initialize(&scene->image_bank, state);
    u32 image_count = dynarray_length(scene->payload.images);
    image2D_config* image_configs = etallocate(sizeof(image2D_config) * image_count, MEMORY_TAG_SCENE);
    for (u32 i = 0; i < image_count; ++i) {
        import_image image = scene->payload.images[i];
        image_configs[i] = (image2D_config) {
            .name = image.name,
            .width = image.width,
            .height = image.height,
            .data = image.data,
        };
    }
    for (u32 i = 0; i < image_count; ++i) {
        image2D_submit(scene->image_bank, &image_configs[i]);
    }
    etfree(image_configs, sizeof(image2D_config) * image_count, MEMORY_TAG_SCENE);

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

    // TEMP: More robust shadow mapping code
    VkExtent3D shadow_map_resolution = {
        .width = 2048,
        .height = 2048,
        .depth = 1};
    image2D_create(
        state,
        shadow_map_resolution,
        VK_FORMAT_D32_SFLOAT,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &scene->shadow_map);
    SET_DEBUG_NAME(state, VK_OBJECT_TYPE_IMAGE, scene->shadow_map.handle, "ShadowMapImage");
    SET_DEBUG_NAME(state, VK_OBJECT_TYPE_IMAGE_VIEW, scene->shadow_map.view, "ShadowMapImageView");
    VkSamplerCreateInfo shadow_map_sampler_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
        .maxAnisotropy = 1.0f,
        .minLod = 0.0f,
        .maxLod = 1.0f,
        .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK};
    VK_CHECK(vkCreateSampler(
        state->device.handle,
        &shadow_map_sampler_info,
        state->allocator,
        &scene->shadow_map_sampler));
    SET_DEBUG_NAME(state, VK_OBJECT_TYPE_SAMPLER, scene->shadow_map_sampler, "ShadowMapSampler");
    // TEMP: END

    // Texture defaults using default samplers and images from renderer_state
    VkDescriptorImageInfo white_texture_info = {
        .sampler = state->linear_smpl,
        .imageView = state->default_white.view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    VkWriteDescriptorSet white_texture_write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = 0,
        .descriptorCount = 1,
        .dstArrayElement = RESERVED_TEXTURE_WHITE_INDEX,
        .dstBinding = SCENE_SET_TEXTURES_BINDING,
        .dstSet = scene->scene_set,
        .pImageInfo = &white_texture_info,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    };
    VkDescriptorImageInfo black_texture_info = {
        .sampler = state->linear_smpl,
        .imageView = state->default_black.view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    VkWriteDescriptorSet black_texture_write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = 0,
        .descriptorCount = 1,
        .dstArrayElement = RESERVED_TEXTURE_BLACK_INDEX,
        .dstBinding = SCENE_SET_TEXTURES_BINDING,
        .dstSet = scene->scene_set,
        .pImageInfo = &black_texture_info,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    };
    VkDescriptorImageInfo normal_texture_info = {
        .sampler = state->linear_smpl,
        .imageView = state->default_normal.view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    VkWriteDescriptorSet normal_texture_write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = 0,
        .descriptorCount = 1,
        .dstArrayElement = RESERVED_TEXTURE_NORMAL_INDEX,
        .dstBinding = SCENE_SET_TEXTURES_BINDING,
        .dstSet = scene->scene_set,
        .pImageInfo = &normal_texture_info,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    };
    VkDescriptorImageInfo shadow_map_texture_info = {
        .sampler = scene->shadow_map_sampler,
        .imageView = scene->shadow_map.view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    VkWriteDescriptorSet shadow_map_texture_write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = 0,
        .descriptorCount = 1,
        .dstArrayElement = RESERVED_TEXTURE_SHADOW_MAP_INDEX,
        .dstBinding = SCENE_SET_TEXTURES_BINDING,
        .dstSet = scene->scene_set,
        .pImageInfo = &shadow_map_texture_info,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    };
    VkWriteDescriptorSet reserved_texture_writes[RESERVED_TEXTURE_INDEX_COUNT] = {
        white_texture_write,
        black_texture_write,
        normal_texture_write,
        shadow_map_texture_write,
    };
    vkUpdateDescriptorSets(
        state->device.handle,
        RESERVED_TEXTURE_INDEX_COUNT,
        reserved_texture_writes,
        /* copy count */ 0,
        /* copies */ NULL
    );

    // Textures
    u32 texture_count = dynarray_length(scene->payload.textures);
    for (u32 i = RESERVED_TEXTURE_INDEX_COUNT; i < texture_count; ++i) {
        scene_texture_set(scene, i, scene->payload.textures[i].image_id, scene->payload.textures[i].sampler_id);
    }

    // Pipeline Creation 
    scene->push_range = (VkPushConstantRange) {
        .offset = 0,
        .size = sizeof(push_constant),
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
    };

    VkPipelineLayoutCreateInfo set0_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = 0,
        .setLayoutCount = 1,
        .pSetLayouts = &scene->scene_set_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &scene->push_range};
    VK_CHECK(vkCreatePipelineLayout(
        state->device.handle,
        &set0_layout_info,
        state->allocator,
        &scene->set0_layout));
    SET_DEBUG_NAME(state, VK_OBJECT_TYPE_PIPELINE_LAYOUT, scene->set0_layout, "DescriptorSet0PipelineLayout");

    const char* draw_gen_path = "assets/shaders/draws.comp.spv.opt";
    shader draw_gen;
    if (!load_shader(state, draw_gen_path, &draw_gen)) {
        ETFATAL("Unable to load draw generation shader.");
        return false;
    }
    VkPipelineShaderStageCreateInfo draw_stage_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = 0,
        .pName = draw_gen.entry_point,
        .stage = draw_gen.stage,
        .module = draw_gen.module};
    VkComputePipelineCreateInfo draw_pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = 0,
        .layout = scene->set0_layout,
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
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &scene->push_range,
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
    scene->mat_pipes = etallocate(sizeof(mat_pipe) * scene->mat_pipe_count, MEMORY_TAG_SCENE);
    for (u32 i = 0; i < scene->mat_pipe_count; ++i) {
        if (!mat_pipe_init(&scene->mat_pipes[i], scene, state, &scene->mat_pipe_configs[i])) {
            ETERROR(
                "Unable to initialize material pipeline with vertex shader %s & fragment shader %s.",
                scene->mat_pipe_configs[i].vert_path,
                scene->mat_pipe_configs[i].frag_path
            );
            return false;
        }
        VkDeviceAddress mat_draws_addr = buffer_get_address(state, &scene->mat_pipes[i].draws_buffer);
        etcopy_memory((VkDeviceAddress*)draw_buffer_addresses + i, &mat_draws_addr, sizeof(VkDeviceAddress));
    }

    // TEMP: Quick and dirty placement of this data
    scene->data.max_draw_count = dynarray_length(scene->objects);
    scene->data.alpha_cutoff = 0.5f;
    scene->data.shadow_draw_id = scene->mat_pipe_count;
    scene->data.shadow_map_id = RESERVED_TEXTURE_SHADOW_MAP_INDEX;
    // TEMP: END

    // TEMP: Shadow mapping placement
    buffer_create(
        state,
        sizeof(draw_command) * MAX_DRAW_COMMANDS,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &scene->shadow_draws);
    scene->data.shadow_draw_id = scene->mat_pipe_count;
    VkDeviceAddress shadow_draws_addr = buffer_get_address(state, &scene->shadow_draws);
    etcopy_memory((VkDeviceAddress*)draw_buffer_addresses + scene->mat_pipe_count, &shadow_draws_addr, sizeof(VkDeviceAddress));
    vkUnmapMemory(state->device.handle, scene->draws_buffer.memory);

    shader shadow_draw_gen;
    if (!load_shader(state, "assets/shaders/shadow_draws.comp.spv.opt", &shadow_draw_gen)) {
        ETFATAL("Unable to load the shadow map draw command gerneration shader.");
        return false;
    }
    VkPipelineShaderStageCreateInfo shadow_draw_stage_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = 0,
        .pName = shadow_draw_gen.entry_point,
        .stage = shadow_draw_gen.stage,
        .module = shadow_draw_gen.module};
    VkComputePipelineCreateInfo shadow_draw_pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = 0,
        .layout = scene->set0_layout,
        .stage = shadow_draw_stage_info};
    VK_CHECK(vkCreateComputePipelines(
        state->device.handle,
        VK_NULL_HANDLE,
        /* CreateInfoCount */ 1,
        &shadow_draw_pipeline_info,
        state->allocator,
        &scene->shadow_draw_gen_pipeline));
    SET_DEBUG_NAME(state, VK_OBJECT_TYPE_PIPELINE, scene->shadow_draw_gen_pipeline, "ShadowDrawGenerationPipeline");
    unload_shader(state, &shadow_draw_gen);

    // TODO: Light passing through alpha-mask texture
    shader shadow_map_vert;
    if (!load_shader(state, "assets/shaders/shadow.vert.spv.opt", &shadow_map_vert)) {
        ETFATAL("Unable to load the shadow map vertex shader.");
        return false;
    };
    // TODO: END

    pipeline_builder builder = pipeline_builder_create();
    builder.layout = scene->set0_layout;
    pipeline_builder_set_vertex_only(&builder, shadow_map_vert);
    pipeline_builder_set_input_topology(&builder, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipeline_builder_set_polygon_mode(&builder, VK_POLYGON_MODE_FILL);

    pipeline_builder_set_cull_mode(&builder, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    pipeline_builder_set_multisampling_none(&builder);

    pipeline_builder_disable_blending(&builder);
    pipeline_builder_enable_depthtest(&builder, true, VK_COMPARE_OP_GREATER_OR_EQUAL);

    pipeline_builder_set_depth_attachment_format(&builder, scene->shadow_map.format);
    scene->shadow_pipeline = pipeline_builder_build(&builder, state);
    SET_DEBUG_NAME(state, VK_OBJECT_TYPE_PIPELINE, scene->shadow_pipeline, "ShadowMapPipeline");
    pipeline_builder_destroy(&builder);

    unload_shader(state, &shadow_map_vert);
    // TEMP: Shadow Mapping end

    // TEMP: Skin stuff placement
    shader skinning_comp;
    if (!load_shader(state, "assets/shaders/skinning.comp.spv.opt", &skinning_comp)) {
        ETFATAL("Unable to load the skinning compute shader.");
        return false;
    }

    VkPipelineShaderStageCreateInfo skinning_comp_stage_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pName = skinning_comp.entry_point,
        .stage = skinning_comp.stage,
        .module = skinning_comp.module};
    VkComputePipelineCreateInfo skinning_comp_pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .layout = scene->set0_layout,
        .stage = skinning_comp_stage_info};
    VK_CHECK(vkCreateComputePipelines(
        state->device.handle,
        VK_NULL_HANDLE,
        /* CreateInfoCount */ 1,
        &skinning_comp_pipeline_info,
        state->allocator,
        &scene->skinning_pipeline));
    SET_DEBUG_NAME(state, VK_OBJECT_TYPE_PIPELINE, scene->skinning_pipeline, "ComputeSkinningPipeline");
    unload_shader(state, &skinning_comp);

    u32 skin_inst_count = dynarray_length(scene->skins);
    for (u32 i = 0; i < skin_inst_count; ++i) {
        u32 joint_count = dynarray_length(scene->payload.skins[scene->skins[i].skin].joint_indices);
        buffer_create_data(
            state,
            scene->payload.skins[scene->skins[i].skin].joint_indices,
            sizeof(u32) * joint_count,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            &scene->skins[i].joints_buffer);
        SET_DEBUG_NAME(state, VK_OBJECT_TYPE_BUFFER, scene->skins[i].joints_buffer.handle, "JointIndexBuffer");
        scene->skins[i].joints_addr = buffer_get_address(state, &scene->skins[i].joints_buffer);
    }
    // TEMP: END
    return true;
}

void scene_renderer_shutdown(scene* scene, renderer_state* state) {
    // NOTE: This is needed to make sure objects are not destroyed while in use
    vkDeviceWaitIdle(state->device.handle);

    buffer_destroy(state, &scene->index_buffer);
    buffer_destroy(state, &scene->scene_uniforms);
    buffer_destroy(state, &scene->counts_buffer);
    buffer_destroy(state, &scene->draws_buffer);
    buffer_destroy(state, &scene->object_buffer);
    buffer_destroy(state, &scene->geometry_buffer);
    buffer_destroy(state, &scene->vertex_buffer);
    buffer_destroy(state, &scene->skin_vertex_buffer);
    buffer_destroy(state, &scene->transform_buffer);

    vkDestroyPipeline(state->device.handle, scene->draw_gen_pipeline, state->allocator);
    vkDestroyPipelineLayout(state->device.handle, scene->set0_layout, state->allocator);
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

    // Shadow Pass shutdown
    buffer_destroy(state, &scene->shadow_draws);
    image_destroy(state, &scene->shadow_map);
    vkDestroySampler(state->device.handle, scene->shadow_map_sampler, state->allocator);
    vkDestroyPipeline(state->device.handle, scene->shadow_draw_gen_pipeline, state->allocator);
    vkDestroyPipeline(state->device.handle, scene->shadow_pipeline, state->allocator);

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
        vkUnmapMemory(state->device.handle, scene->staging_buffers[i].memory);
        buffer_destroy(state, &scene->staging_buffers[i]);
    }
    etfree(scene->graphics_command_buffers, sizeof(VkCommandBuffer) * frame_overlap, MEMORY_TAG_SCENE);
    etfree(scene->graphics_pools, sizeof(VkCommandPool) * frame_overlap, MEMORY_TAG_SCENE);
    etfree(scene->render_fences, sizeof(VkFence) * frame_overlap, MEMORY_TAG_SCENE);
    etfree(scene->staging_buffers, sizeof(buffer) * frame_overlap, MEMORY_TAG_SCENE);
    etfree(scene->staging_mapped, sizeof(u8*) * frame_overlap, MEMORY_TAG_SCENE);

    image_destroy(state, &scene->depth_image);
    image_destroy(state, &scene->render_image);
}

// NOTE: Once there is a staging & memory management system we can just create 
// buffers as necessary without waiting on per-frame staging to be available
b8 scene_render(scene* scene, renderer_state* state) {
    // TEMP: Until some kind of staging buffer system is is place
    u32 frame_index = state->swapchain.frame_index;
    VkCommandBuffer cmd = scene->graphics_command_buffers[frame_index];
    scene->staging_mapped[frame_index];

    u32 transforms_bytes = sizeof(m4s) * scene->transforms.capacity;
    etcopy_memory(
        scene->staging_mapped[frame_index],
        scene->transforms.global,
        transforms_bytes
    );
    etcopy_memory(
        (u8*)scene->staging_mapped[frame_index] + transforms_bytes,
        &scene->data,
        sizeof(scene_data)
    );
    
    VkBufferCopy2 transform_copy = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
        .srcOffset = 0,
        .dstOffset = 0,
        .size = transforms_bytes,
    };
    VkCopyBufferInfo2 transform_copy_info = {
        .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
        .srcBuffer = scene->staging_buffers[frame_index].handle,
        .dstBuffer = scene->transform_buffer.handle,
        .regionCount = 1,
        .pRegions = &transform_copy,
    };
    VkBufferCopy2 scene_uniforms_copy = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
        .srcOffset = transforms_bytes,
        .dstOffset = 0,
        .size = sizeof(scene_data),
    };
    VkCopyBufferInfo2 scene_uniforms_copy_info = {
        .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
        .srcBuffer = scene->staging_buffers[frame_index].handle,
        .dstBuffer = scene->scene_uniforms.handle,
        .regionCount = 1,
        .pRegions = &scene_uniforms_copy,
    };

    vkCmdCopyBuffer2(cmd, &transform_copy_info);
    vkCmdCopyBuffer2(cmd, &scene_uniforms_copy_info);

    buffer_barrier(cmd, scene->scene_uniforms.handle, /* Offset: */ 0, VK_WHOLE_SIZE,
        VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_UNIFORM_READ_BIT,
        VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT
    );
    buffer_barrier(cmd, scene->transform_buffer.handle, /* Offset: */ 0, VK_WHOLE_SIZE,
        VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT
    );

    vkCmdFillBuffer(cmd,
        scene->counts_buffer.handle,
        /* Offset: */ 0,
        sizeof(u32) * (scene->mat_pipe_count + /* TEMP: shadow map draws buffer */ 1),
        (u32)0);
    buffer_barrier(cmd, scene->counts_buffer.handle, /* Offset: */ 0, VK_WHOLE_SIZE,
        VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT
    );
    // TEMP: END
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

void draw_command_generation(renderer_state* state, scene* scene, VkCommandBuffer cmd) {
    // Shadow draw command generation compute
    u32 object_count = dynarray_length(scene->objects);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, scene->set0_layout, 0, 1, &scene->scene_set, 0, NULL);

    u32 group_count_x = (u32)ceil((f32)object_count / 32.0f);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, scene->shadow_draw_gen_pipeline);
    vkCmdDispatch(cmd, group_count_x, 1, 1);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, scene->draw_gen_pipeline);
    vkCmdDispatch(cmd, group_count_x, 1, 1);

    // Wait for shadow draw generation before reading from indirect command buffer
    buffer_barrier(
        cmd, scene->shadow_draws.handle, /* Offset */ 0, VK_WHOLE_SIZE,
        VK_ACCESS_2_SHADER_WRITE_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT);
    buffer_barrier(
        cmd, scene->counts_buffer.handle, sizeof(u32) * scene->mat_pipe_count, sizeof(u32),
        VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT
    );

    // NOTE: Clean this up
    for (u32 i = 0; i < scene->mat_pipe_count; ++i) {
        buffer_barrier(
            cmd, scene->mat_pipes[i].draws_buffer.handle, /* Offset */ 0, VK_WHOLE_SIZE,
            VK_ACCESS_2_SHADER_WRITE_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT
        );
    }
    buffer_barrier(
        cmd, scene->counts_buffer.handle, /* offset: */ 0, sizeof(u32) * scene->mat_pipe_count,
        VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT
    );
    // NOTE: END
}

void compute_skinning(renderer_state* state, scene* scene, VkCommandBuffer cmd) {
    push_constant push = {0};
    u32 skin_inst_count = dynarray_length(scene->skins);
    for (u32 i = 0; i < skin_inst_count; ++i) {
        u32 mesh_inst_count = dynarray_length(scene->skins[i].meshes);
        for (u32 j = 0; j < mesh_inst_count; j++) {
            skin_mesh_inst mesh_inst = scene->skins[i].meshes[j];
            import_mesh mesh = scene->payload.meshes[mesh_inst.mesh];
            push.u64s[0] = scene->skins[i].joints_addr;
            push.u32s[6] = mesh.vertex_offset;
            push.u32s[7] = mesh_inst.vertex_offset;
            push.u32s[8] = mesh.vertex_count;

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, scene->skinning_pipeline);
            vkCmdPushConstants(
                cmd,
                scene->set0_layout,
                VK_SHADER_STAGE_COMPUTE_BIT,
                0,
                sizeof(push_constant),
                &push);
            u32 group_count_x = (u32)ceil((f32)mesh.vertex_count / 32.0f);
            vkCmdDispatch(cmd, group_count_x, 1, 1);
        }
    }

    u32 barrier_offset = sizeof(vertex) * dynarray_length(scene->vertices);
    u32 barrier_size = sizeof(vertex) * scene->extra_vertex_count;
    buffer_barrier(cmd, scene->vertex_buffer.handle, barrier_offset, barrier_size,
        VK_ACCESS_2_SHADER_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT
    );
}

void shadow_pass(renderer_state* state, scene* scene, VkCommandBuffer cmd) {
    VkRenderingAttachmentInfo depth_attachment = init_depth_attachment_info(
        scene->shadow_map.view, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
    
    VkRect2D render_rect = {
        .extent = {
            .width = scene->shadow_map.extent.width,
            .height = scene->shadow_map.extent.height,
        },
    };
    VkRenderingInfo render_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .pNext = 0,
        .renderArea = render_rect,
        .layerCount = 1,
        .pDepthAttachment = &depth_attachment,
        .pStencilAttachment = 0,
    };

    vkCmdBeginRendering(cmd, &render_info);

    VkViewport viewport = {
        .width = render_rect.extent.width,
        .height = render_rect.extent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f};
    VkRect2D scissor = render_rect;
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindIndexBuffer(cmd, scene->index_buffer.handle, 0, VK_INDEX_TYPE_UINT32);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, scene->set0_layout, 0, 1, &scene->scene_set, 0, NULL);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, scene->shadow_pipeline);

    vkCmdDrawIndexedIndirectCount(cmd,
        scene->shadow_draws.handle,
        /* Offset: */ 0,
        scene->counts_buffer.handle,
        sizeof(u32) * scene->mat_pipe_count,
        MAX_DRAW_COMMANDS,
        sizeof(draw_command)
    );

    vkCmdEndRendering(cmd);
}

void geometry_pass(renderer_state* state, scene* scene, VkCommandBuffer cmd) {
    VkClearValue clear_color = {
        .color = {.3f,0.f,.2f,0.f},
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

    // Generate draw commands
    draw_command_generation(state, scene, frame_cmd);

    compute_skinning(state, scene, frame_cmd);
    
    // Get shadow map ready to render to
    image_barrier(frame_cmd, scene->shadow_map.handle, scene->shadow_map.aspects,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        VK_ACCESS_2_NONE, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT);
    shadow_pass(state, scene, frame_cmd);
    // Do not sample from shadow map until the shadow pass has reached end of depth writing
    image_barrier(frame_cmd, scene->shadow_map.handle, scene->shadow_map.aspects,
        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
        VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);

    // Get color attachment and depth attachment ready to be rendered to
    image_barrier(frame_cmd, scene->render_image.handle, scene->render_image.aspects,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_ACCESS_2_NONE, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
        VK_PIPELINE_STAGE_2_NONE, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);
    image_barrier(frame_cmd, scene->depth_image.handle, scene->depth_image.aspects,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        VK_ACCESS_2_NONE, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT);
    geometry_pass(state, scene, frame_cmd);

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

// TODO: New abstraction than image_manager & this function to handle images and textures
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
    switch (key) {
        case KEY_L:
            light_offset += 2.0f;
            break;
        case KEY_K:
            light_offset -= 2.0f; 
            break;
        case KEY_J:
            light_dynamic = !light_dynamic;
            break;
        case KEY_Q:
            sun_pov = true;
            break;
        case KEY_E:
            sun_pov = false;
            break;
        case KEY_F:
            sun_pov_persp = false;
            break;
        case KEY_G:
            sun_pov_persp = true;
            break;
        case KEY_Y:
            s->data.debug_view = (s->data.debug_view + 1) % DEBUG_VIEW_TYPE_MAX;
            break;
        case KEY_T: {
            s->data.debug_view = (s->data.debug_view) ? s->data.debug_view - 1 : DEBUG_VIEW_TYPE_MAX - 1;
            break;
        }
    }
    return false;
}

// TEMP: Recursive helper
void add_node_to_transforms(u32 node_index, u32* nodes_to_transforms, import_node* nodes, transforms transforms) {
    import_node node = nodes[node_index];
    transform node_transform = matrix_to_transform(node.local_transform);
    u32 parent_transform_index = nodes_to_transforms[node.parent_index];
    nodes_to_transforms[node_index] = transforms_add_child(transforms, parent_transform_index, node_transform);

    u32 children_count = dynarray_length(node.children_indices);
    for (u32 i = 0; i < children_count; ++i) {
        add_node_to_transforms(node.children_indices[i], nodes_to_transforms, nodes, transforms);
    }
}