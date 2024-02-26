#include "scene.h"
#include "scene/scene_private.h"

#include "data_structures/dynarray.h"

#include "memory/etmemory.h"
#include "core/etstring.h"
#include "core/logger.h"

// TEMP: Until events refactor
#include "core/events.h"
#include "core/input.h"
// TEMP: END

#include "resources/mesh_manager.h"
#include "resources/image_manager.h"
#include "resources/material_manager.h"
#include "resources/resource_private.h"

// TEMP: Make this renderer implementation agnostic
#include "renderer/src/vk_types.h"
#include "renderer/src/renderer.h"
#include "renderer/src/utilities/vkinit.h"
#include "renderer/src/buffer.h"
#include "renderer/src/descriptor.h"
#include "renderer/src/shader.h"
#include "renderer/src/pipeline.h"
#include "renderer/src/renderables.h"
#include "resources/importers/gltfimporter.h"
// TEMP: END

// TEMP: Bindless draw commands stuff
#define MAX_DRAW_COMMANDS 8192
// TEMP: END

// TEMP: Until events refactor
static b8 scene_on_key_event(u16 code, void* scne, event_data data);
// TEMP: END

// TEMP: Bindless testing
static void scene_init_bindless(scene* scene);
static void scene_shutdown_bindless(scene* scene);
// TEMP: END

/** NOTE: Implementation details
 * Currently the scene descriptor set is hardcoded as a singular
 * uniform buffer that matches struct scene_data.
 */
b8 scene_initalize(scene** scn, renderer_state* state) {
    // HACK:TODO: Make configurable & from application
    const char* path = "build/assets/gltf/structure.glb";
    // HACK:TODO: END

    scene* new_scene = etallocate(sizeof(struct scene), MEMORY_TAG_SCENE);
    new_scene->state = state;
    new_scene->name = str_duplicate_allocate(path);

    mesh_manager_initialize(&new_scene->mesh_bank, state);
    image_manager_initialize(&new_scene->image_bank, state);
    material_manager_initialize(&new_scene->material_bank, state);

    camera_create(&new_scene->cam);
    new_scene->cam.position = (v3s){.raw = {0.0f, 0.0f, 5.0f}};

    // NOTE: This will be passed a config when serialization is implemented
    v4s a_color = { .raw = {.1f, .1f, .1f, 3.f}};
    v4s l_color = { .raw = {1.f, 1.f, 1.f, 50.f}};
    
    new_scene->data.ambient_color = a_color;
    new_scene->data.light_color = l_color;

    // TEMP: Bindless
    scene_init_bindless(new_scene);
    // TEMP: END
    // HACK:TEMP: Serialize and deserialization of a scene file
    if (!import_gltf(new_scene, path, state)) {
        ETERROR("Error loading gltf file initializing scene.");
        return false;
    }
    // HACK:TEMP: END

    event_observer_register(EVENT_CODE_KEY_RELEASE, new_scene, scene_on_key_event);
    *scn = new_scene;
    return true;
}

// TEMP: Scene parameters here are currently hard-coded for an imported gltf file.
// Eventually the material buffers will be handled by the material manager
// The samplers will have some kind of system as well to manage them
void scene_shutdown(scene* scene) {
    renderer_state* state = scene->state;

    event_observer_deregister(EVENT_CODE_KEY_RELEASE, scene, scene_on_key_event);

    dynarray_destroy(scene->top_nodes);

    for (u32 i = 0; i < scene->mesh_node_count; ++i) {
        str_duplicate_free(scene->mesh_nodes[i].base.name);
        mesh_node_destroy(&scene->mesh_nodes[i]);
    }
    etfree(scene->mesh_nodes, sizeof(mesh_node) * scene->mesh_node_count, MEMORY_TAG_SCENE);

    for (u32 i = 0; i < scene->node_count; ++i) {
        str_duplicate_free(scene->nodes[i].name);
        node_destroy(&scene->nodes[i]);
    }
    etfree(scene->nodes, sizeof(node) * scene->node_count, MEMORY_TAG_SCENE);

    // TODO: Put renderer objects to destroy in a deletion queue
    // to insure they are destroyed when unused.
    vkDeviceWaitIdle(state->device.handle);
    // TODO: END

    // TODO: Modify to use GPU Driven architecture
    mesh_manager_shutdown(scene->mesh_bank);
    material_manager_shutdown(scene->material_bank);
    buffer_destroy(state, &scene->material_buffer);
    image_manager_shutdown(scene->image_bank);
    // TODO: END

    for (u32 i = 0; i < scene->sampler_count; ++i)
        vkDestroySampler(state->device.handle, scene->samplers[i], state->allocator);
    etfree(scene->samplers, sizeof(VkSampler) * scene->sampler_count, MEMORY_TAG_SCENE);

    scene_shutdown_bindless(scene);

    camera_destroy(&scene->cam);

    str_duplicate_free(scene->name);
    
    etfree(scene, sizeof(struct scene), MEMORY_TAG_SCENE);
}

// TEMP: For testing and debugging
static f32 light_offset = 2.0f;
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

// TEMP: Bindless testing
void scene_init_bindless(scene* scene) {
    scene->op_draws_count = 0;
    scene->tp_draws_count = 0;
    scene->opaque_draws = dynarray_create(1, sizeof(draw_command));
    scene->transparent_draws = dynarray_create(1, sizeof(draw_command));

    // TODO: Read from shader reflection data. 
    // Even though SET 0 is reserved for engine and draw calls,
    // so I dont have to find all places to change when changing the engine.

    // For use in VkDescriptorSetLayoutBindingFlagsCreateInfo for each set
    const VkDescriptorBindingFlags flags = 
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
        VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT;
 
    // Set 0: Scene set layout (engine specific). The set itself will be allocated on the fly
    VkDescriptorBindingFlags ssbf = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT;
    VkDescriptorBindingFlags scene_set_binding_flags[] = {
        [0] = ssbf,
        [1] = ssbf,
        [2] = ssbf | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT,
    };
    VkDescriptorSetLayoutBindingFlagsCreateInfo scene_binding_flags_create_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .pNext = 0,
        .bindingCount = 3,
        .pBindingFlags = scene_set_binding_flags,
    };
    VkDescriptorSetLayoutBinding scene_set_bindings[] = {
        [0] = {
            .binding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = NULL,
        },
        [1] = {
            .binding = 1,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = NULL,
        },
        [2] = {
            .binding = 2,
            .descriptorCount = scene->state->device.properties_12.maxDescriptorSetUpdateAfterBindSampledImages,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = NULL,
        },
    };
    VkDescriptorSetLayoutCreateInfo scene_layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &scene_binding_flags_create_info,
        .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
        .bindingCount = 3,
        .pBindings = scene_set_bindings,
    };
    VK_CHECK(vkCreateDescriptorSetLayout(
        scene->state->device.handle,
        &scene_layout_info,
        scene->state->allocator,
        &scene->scene_layout
    ));

    // Setup global descriptor
    scene->push_constant.offset = 0;
    scene->push_constant.size = sizeof(VkDeviceAddress) + sizeof(VkDeviceAddress);
    scene->push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    // Set 1: Material layout (material specific). Arrays of each element
    dsl_builder mat_builder = descriptor_set_layout_builder_create();
    mat_builder.layout_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    descriptor_set_layout_builder_add_binding(
        &mat_builder,
        /* Binding: */ 0,
        scene->state->device.properties_12.maxDescriptorSetUpdateAfterBindUniformBuffers - 30,  // HACK:TEMP: FIX THIS
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    const VkDescriptorBindingFlags mat_binding_flags[] = {
        [0] = flags | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT,
    };
    VkDescriptorSetLayoutBindingFlagsCreateInfo mat_binding_flags_cinfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .pNext = 0,
        .bindingCount = 1,
        .pBindingFlags = mat_binding_flags};
    mat_builder.layout_info.pNext = &mat_binding_flags_cinfo;
    scene->material_layout = descriptor_set_layout_builder_build(
        &mat_builder,
        scene->state);
    descriptor_set_layout_builder_destroy(&mat_builder);

    // DescriptorPool
    VkDescriptorPoolSize sizes[] = {
        [0] = {
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = MAX_IMAGE_COUNT * 3,
        },
        [1] = {
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1000 + MAX_MATERIAL_COUNT,
        },
        [2] = {
            .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1000,
        },
    };
    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = 0,
        .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
        .maxSets = 4,
        .poolSizeCount = 3,
        .pPoolSizes = sizes
    };
    VK_CHECK(vkCreateDescriptorPool(
        scene->state->device.handle,
        &pool_info,
        scene->state->allocator,
        &scene->bindless_pool 
    ));

    scene->scene_sets = etallocate(
        sizeof(VkDescriptorSet) * scene->state->swapchain.image_count,
        MEMORY_TAG_SCENE
    );
    VkDescriptorSetLayout scene_layouts[] = {
        [0] = scene->scene_layout,
        [1] = scene->scene_layout,
        [2] = scene->scene_layout,
    };
    u32 scene_image_counts[] = {
        [0] = MAX_IMAGE_COUNT,
        [1] = MAX_IMAGE_COUNT,
        [2] = MAX_IMAGE_COUNT,
    };
    VkDescriptorSetVariableDescriptorCountAllocateInfo scene_descriptor_count_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
        .descriptorSetCount = 3,
        .pDescriptorCounts = scene_image_counts,
    };
    VkDescriptorSetAllocateInfo scene_set_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = &scene_descriptor_count_info,
        .descriptorPool = scene->bindless_pool,
        .descriptorSetCount = 3,
        .pSetLayouts = scene_layouts,
    };
    VK_CHECK(vkAllocateDescriptorSets(
        scene->state->device.handle,
        &scene_set_alloc_info,
        scene->scene_sets
    ));

    // Create buffers
    scene->draw_buffers = etallocate(sizeof(buffer) * scene->state->swapchain.image_count, MEMORY_TAG_SCENE);
    scene->scene_uniforms = etallocate(sizeof(buffer) * scene->state->swapchain.image_count, MEMORY_TAG_SCENE);
    for (u32 i = 0; i < scene->state->swapchain.image_count; ++i) {
        buffer_create(
            scene->state,
            sizeof(scene_data),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &scene->scene_uniforms[i]
        );
        // TEMP: Better way of getting the length for the draw commands buffer & device local bit
        buffer_create(
            scene->state,
            sizeof(draw_command) * MAX_DRAW_COMMANDS * 2,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            &scene->draw_buffers[i]);
        // TEMP: END
    }

    // Write buffers to DescriptorSets
    for (u32 i = 0; i < scene->state->swapchain.image_count; ++i) {
        VkDescriptorBufferInfo uniform_buffer_info = {
            .buffer = scene->scene_uniforms[i].handle,
            .offset = 0,
            .range = VK_WHOLE_SIZE,
        };
        VkWriteDescriptorSet uniform_write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = 0,
            .descriptorCount = 1,
            .dstArrayElement = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .dstSet = scene->scene_sets[i],
            .dstBinding = 0,
            .pBufferInfo = &uniform_buffer_info,
        };
        VkDescriptorBufferInfo draws_buffer_info = {
            .buffer = scene->draw_buffers[i].handle,
            .offset = 0,
            .range = VK_WHOLE_SIZE,
        };
        VkWriteDescriptorSet draws_buffer_write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = 0,
            .descriptorCount = 1,
            .dstArrayElement = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .dstSet = scene->scene_sets[i],
            .dstBinding = 1,
            .pBufferInfo = &draws_buffer_info,
        };
        VkWriteDescriptorSet writes[] = {
            [0] = uniform_write,
            [1] = draws_buffer_write,
        };
        vkUpdateDescriptorSets(
            scene->state->device.handle,
            /* writeCount: */ 2,
            writes,
            /* copyCount: */ 0,
            /* copies: */ 0 
        );
    }

    // Material Descriptor Set
    u32 material_uniform_buffer_count = MAX_MATERIAL_COUNT;
    VkDescriptorSetVariableDescriptorCountAllocateInfo material_descriptor_count_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
        .descriptorSetCount = 1,
        .pDescriptorCounts = &material_uniform_buffer_count,
    };
    VkDescriptorSetAllocateInfo mat_set_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = &material_descriptor_count_info,
        .descriptorPool = scene->bindless_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &scene->material_layout,
    };
    VK_CHECK(vkAllocateDescriptorSets(
        scene->state->device.handle,
        &mat_set_info,
        &scene->material_set
    ));

    scene->material_count = 0;
    for (u32 i = 0; i < MAX_MATERIAL_COUNT; ++i) {
        scene->materials[i] = (material_2){.id = {.blueprint_id = INVALID_ID, .instance_id = INVALID_ID}, .pass = MATERIAL_PASS_OTHER};
    }

    // Load bindless shaders
    const char* vertex_path = "build/assets/shaders/bindless_mesh_mat.vert.spv.opt";
    const char* fragment_path = "build/assets/shaders/bindless_mesh_mat.frag.spv.opt";

    shader bindless_vertex;
    if (!load_shader(scene->state, vertex_path, &bindless_vertex)) {
        ETERROR("Error loading shader %s.", vertex_path);
        return;
    }

    shader bindless_fragment;
    if (!load_shader(scene->state, fragment_path, &bindless_fragment)) {
        ETERROR("Error loading shader %s.", fragment_path);
        return;
    }

    // Create Pipeline for bindless shaders
    VkDescriptorSetLayout pipeline_ds_layouts[] = {
        [0] = scene->scene_layout,
        [1] = scene->material_layout,
    };
    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = 0,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &scene->push_constant,
        .setLayoutCount = 2,
        .pSetLayouts = pipeline_ds_layouts,
    };
    VK_CHECK(vkCreatePipelineLayout(
        scene->state->device.handle,
        &pipeline_layout_create_info,
        scene->state->allocator,
        &scene->pipeline_layout
    ));

    // Opaque pipeline
    pipeline_builder builder = pipeline_builder_create();
    pipeline_builder_set_shaders(&builder, bindless_vertex, bindless_fragment);
    pipeline_builder_set_input_topology(&builder, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipeline_builder_set_polygon_mode(&builder, VK_POLYGON_MODE_FILL);
    pipeline_builder_set_cull_mode(&builder, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipeline_builder_set_multisampling_none(&builder);
    pipeline_builder_disable_blending(&builder);
    pipeline_builder_enable_depthtest(&builder, true, VK_COMPARE_OP_GREATER_OR_EQUAL);

    pipeline_builder_set_color_attachment_format(&builder, scene->state->render_image.format);
    pipeline_builder_set_depth_attachment_format(&builder, scene->state->depth_image.format);
    builder.layout = scene->pipeline_layout;
    scene->opaque_pipeline = pipeline_builder_build(&builder, scene->state);
    
    // Transparent pipeline
    pipeline_builder_enable_blending_additive(&builder);
    pipeline_builder_enable_depthtest(&builder, false, VK_COMPARE_OP_GREATER_OR_EQUAL);

    scene->transparent_pipeline = pipeline_builder_build(&builder, scene->state);

    pipeline_builder_destroy(&builder);

    // Unload shaders
    unload_shader(scene->state, &bindless_vertex);
    unload_shader(scene->state, &bindless_fragment);
}

void scene_shutdown_bindless(scene* scene) {
    renderer_state* state = scene->state;
    dynarray_destroy(scene->objects);
    dynarray_destroy(scene->transforms);
    dynarray_destroy(scene->meshes);
    dynarray_destroy(scene->surfaces);
    dynarray_destroy(scene->indices);
    dynarray_destroy(scene->vertices);

    buffer_destroy(state, &scene->bindless_material_buffer);
    buffer_destroy(state, &scene->index_buffer);
    buffer_destroy(state, &scene->vertex_buffer);
    buffer_destroy(state, &scene->transform_buffer);

    vkDestroyPipeline(state->device.handle, scene->opaque_pipeline, state->allocator);
    vkDestroyPipeline(state->device.handle, scene->transparent_pipeline, state->allocator);
    vkDestroyPipelineLayout(state->device.handle, scene->pipeline_layout, state->allocator);

    for (u32 i = 0; i < state->swapchain.image_count; ++i) {
        buffer_destroy(state, scene->scene_uniforms + i);
        buffer_destroy(state, scene->draw_buffers + i);
    }

    etfree(scene->scene_uniforms, sizeof(buffer) * state->swapchain.image_count, MEMORY_TAG_SCENE); 
    etfree(scene->draw_buffers, sizeof(buffer) * state->swapchain.image_count, MEMORY_TAG_SCENE);

    vkDestroyDescriptorPool(
        state->device.handle,
        scene->bindless_pool,
        state->allocator);
    etfree(
        scene->scene_sets,
        sizeof(VkDescriptorSet) * state->swapchain.image_count,
        MEMORY_TAG_SCENE
    );

    vkDestroyDescriptorSetLayout(
        state->device.handle,
        scene->material_layout,
        state->allocator);
    vkDestroyDescriptorSetLayout(
        state->device.handle,
        scene->scene_layout,
        state->allocator);

    dynarray_destroy(scene->transparent_draws);
    dynarray_destroy(scene->opaque_draws);
}

void scene_draw(scene* scene) {
    surface_2* surfaces = scene->surfaces;
    mesh_2* meshes = scene->meshes;

    u32 mesh_count = dynarray_length(meshes);
    for (u64 i = 0; i < mesh_count; ++i) {
        for (u32 j = meshes[i].start_surface; j < meshes[i].start_surface + meshes[i].surface_count; ++j) {
            draw_command draw = {
                .draw = {
                    .firstIndex = surfaces[j].start_index,
                    .instanceCount = meshes[i].instance_count,
                    .indexCount = surfaces[j].index_count,
                    .vertexOffset = meshes[i].vertex_offset,
                    .firstInstance = meshes[i].transform_offset,
                },
                .material_instance_id = surfaces[j].material.id.instance_id,
                .transform_offset = meshes[i].transform_offset,
            };
            switch (surfaces[j].material.pass)
            {
            case MATERIAL_PASS_MAIN_COLOR:
                dynarray_push((void**)&scene->opaque_draws, &draw);                
                break;
            case MATERIAL_PASS_TRANSPARENT:
                dynarray_push((void**)&scene->transparent_draws, &draw);                
                break;
            default:
                ETERROR("Unknown material pass type.");
                break;
            }
        }
    }
}

b8 scene_render(scene* scene) {
    renderer_state* state = scene->state;

    // Copy scene data to the current frames uniform buffer:
    void* mapped_uniform;
    VK_CHECK(vkMapMemory(
        state->device.handle,
        scene->scene_uniforms[state->swapchain.frame_index].memory,
        /* Offset: */ 0,
        VK_WHOLE_SIZE,
        /* Flags: */ 0,
        &mapped_uniform));
    etcopy_memory(mapped_uniform, &scene->data, sizeof(scene_data));
    vkUnmapMemory(state->device.handle, scene->scene_uniforms[state->swapchain.frame_index].memory);

    // Copy created draws over to the draws buffer for indirect drawing:
    scene->op_draws_count = dynarray_length(scene->opaque_draws);
    scene->tp_draws_count = dynarray_length(scene->transparent_draws);
    
    // TEMP: Stop draw count from going over the maximum draws count allocated for the draws buffer
    scene->op_draws_count = (scene->op_draws_count > MAX_DRAW_COMMANDS) ? MAX_DRAW_COMMANDS : scene->op_draws_count;
    scene->tp_draws_count = (scene->tp_draws_count > MAX_DRAW_COMMANDS) ? MAX_DRAW_COMMANDS : scene->tp_draws_count;

    u64 op_draws_size = sizeof(draw_command) * scene->op_draws_count;
    u64 tp_draws_size = sizeof(draw_command) * scene->tp_draws_count;
    // TEMP: END

    void* mapped_draws;
    VK_CHECK(vkMapMemory(
        state->device.handle,
        scene->draw_buffers[state->swapchain.frame_index].memory,
        /* Offset: */ 0,
        op_draws_size + tp_draws_size,
        /* Flags: */ 0,
        &mapped_draws));
    etcopy_memory(mapped_draws, scene->opaque_draws, op_draws_size);
    etcopy_memory((u8*)mapped_draws + op_draws_size, scene->transparent_draws, tp_draws_size);
    vkUnmapMemory(state->device.handle, scene->draw_buffers[state->swapchain.frame_index].memory);

    // Clear draws after copied to draws buffer:
    dynarray_clear(scene->opaque_draws);
    dynarray_clear(scene->transparent_draws);
    return true;
}

b8 scene_frame_begin(scene* scene, renderer_state* state) {
    VkResult result;

    // Wait for the current frame to end rendering by waiting on its render fence
    result = vkWaitForFences(
        state->device.handle,
        /* Fence count: */ 1,
        &state->render_fences[state->swapchain.frame_index],
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
        &state->render_fences[state->swapchain.frame_index]
    ));

    VK_CHECK(vkResetCommandBuffer(state->main_graphics_command_buffers[state->swapchain.frame_index], 0));
    VkCommandBufferBeginInfo begin_info = init_command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(state->main_graphics_command_buffers[state->swapchain.frame_index], &begin_info));
    return true;
}

void draw_geometry(renderer_state* state, scene* scene, VkCommandBuffer cmd) {
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

    VkDescriptorSet sets[] = {
        scene->scene_sets[state->swapchain.frame_index],
        scene->material_set
    };

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, scene->opaque_pipeline);
    
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, scene->pipeline_layout, 0, 2, sets, 0, NULL);

    struct push {
        VkDeviceAddress vertex_buff;
        VkDeviceAddress transform_buff;
    } push = {
        .vertex_buff = scene->vb_addr,
        .transform_buff = scene->tb_addr,
    };
    vkCmdPushConstants(cmd, scene->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(struct push), &push);

    vkCmdBindIndexBuffer(cmd, scene->index_buffer.handle, 0, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexedIndirect(cmd, scene->draw_buffers[state->swapchain.frame_index].handle, 0, scene->op_draws_count, sizeof(draw_command));

    vkCmdEndRendering(cmd);
}

b8 scene_frame_end(scene* scene, renderer_state* state) {
    VkResult result;
    VkCommandBuffer frame_cmd = state->main_graphics_command_buffers[state->swapchain.frame_index];

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
        state->render_fences[state->swapchain.frame_index]);
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

b8 scene_material_set_instance(scene* scene, material_pass pass_type, struct bindless_material_resources* resources) {
    // TODO: Update the material set based on the amount of material instances
    if (scene->material_count >= MAX_MATERIAL_COUNT) {
        ETERROR("Max material count reached.");
        return false;
    }

    VkDescriptorBufferInfo buff_info = {
        .buffer = resources->data_buff,
        .offset = resources->data_buff_offset,
        .range = sizeof(struct bindless_constants),
    };
    VkWriteDescriptorSet buffer_write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .dstArrayElement = scene->material_count,
        .dstSet = scene->material_set,
        .dstBinding = 0,
        .pBufferInfo = &buff_info,
    };

    vkUpdateDescriptorSets(
        scene->state->device.handle,
        /* WriteCount */ 1,
        &buffer_write,
        /* CopyCount: */ 0,
        /* Copies: */ NULL
    );

    material_2 mat = {
        .id = {
            .blueprint_id = 0,
            .instance_id = scene->material_count,
        },
        .pass = pass_type,
    };
    scene->materials[scene->material_count++] = mat;
    return true;
}

material_2 scene_material_get_instance(scene* scene, material_id id) {
    if (id.instance_id > scene->material_count || scene->materials[id.instance_id].id.instance_id == INVALID_ID) {
        ETERROR("Material not present. Returning invalid material.");
        // TODO: Create a default material to return a material_2 struct for
        return (material_2){.id = {.blueprint_id = INVALID_ID, .instance_id = INVALID_ID}, .pass = MATERIAL_PASS_OTHER};
    }
    return scene->materials[id.instance_id];
}

void scene_image_set(scene* scene, u32 img_id, u32 sampler_id) {
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
        .dstArrayElement = img_id,
        .dstBinding = 2,
        .dstSet = VK_NULL_HANDLE,
        .pImageInfo = &image_info,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    };

    img_write.dstSet = scene->scene_sets[0];
    vkUpdateDescriptorSets(
        scene->state->device.handle,
        /* WriteCount: */ 1,
        &img_write,
        /* CopyCount: */ 0,
        /* Copies: */ NULL
    );

    img_write.dstSet = scene->scene_sets[1];
    vkUpdateDescriptorSets(
        scene->state->device.handle,
        /* WriteCount: */ 1,
        &img_write,
        /* CopyCount: */ 0,
        /* Copies: */ NULL
    );

    img_write.dstSet = scene->scene_sets[2];
    vkUpdateDescriptorSets(
        scene->state->device.handle,
        /* WriteCount: */ 1,
        &img_write,
        /* CopyCount: */ 0,
        /* Copies: */ NULL
    );
}
// TEMP: END

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