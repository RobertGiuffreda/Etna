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

// HACK: Seems wrong
static void scene_draw(scene* scene, const m4s top_matrix, draw_context* ctx);

// TEMP: Bindless testing
static void scene_init_bindless(scene* scene);
static void scene_shutdown_bindless(scene* scene);
static void scene_draw_bindless(scene* scene);
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

    // HACK: Better way to wait until resources are unused
    vkDeviceWaitIdle(state->device.handle);
    // HACK: END

    mesh_manager_shutdown(scene->mesh_bank);
    material_manager_shutdown(scene->material_bank);
    buffer_destroy(state, &scene->material_buffer);
    image_manager_shutdown(scene->image_bank);

    for (u32 i = 0; i < scene->sampler_count; ++i)
        vkDestroySampler(state->device.handle, scene->samplers[i], state->allocator);
    etfree(scene->samplers, sizeof(VkSampler) * scene->sampler_count, MEMORY_TAG_SCENE);

    scene_shutdown_bindless(scene);

    camera_destroy(&scene->cam);

    str_duplicate_free(scene->name);
    
    scene->state = 0;
    etfree(scene, sizeof(struct scene), MEMORY_TAG_SCENE);
}

// TEMP: For testing and debugging
static f32 light_offset = 2.0f;
static b8 light_dynamic = true;
// TEMP: END

void scene_update(scene* scene) {
    renderer_state* state = scene->state;

    camera_update(&scene->cam);

    // TODO: Camera should store near and far values & calculate perspective matrix
    // TODO: Scene should register for event system and update camera stuff itself 
    m4s view = camera_get_view_matrix(&scene->cam);
    m4s project = glms_perspective(
        /* fovy */ glm_rad(70.f),
        ((f32)state->window_extent.width/(f32)state->window_extent.height), 
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

void scene_render(scene* scene) {
    renderer_state* state = scene->state;

    // HACK:TEMP: Better way to update scene buffer
    // Set the per frame scene data in state->scene_data_buffers[frame_index]
    void* mapped_scene_data;
    VK_CHECK(vkMapMemory(
        state->device.handle,
        state->scene_data[state->frame_index].memory,
        /* Offset: */ 0,
        sizeof(scene_data),
        /* Flags: */ 0,
        &mapped_scene_data));
    // Cast mapped memory to scene_data pointer to read and modify it
    scene_data* frame_scene_data = (scene_data*)mapped_scene_data;
    *frame_scene_data = scene->data;
    vkUnmapMemory(
        state->device.handle,
        state->scene_data[state->frame_index].memory);
    // HACK:TEMP: END

    scene_draw(scene, glms_mat4_identity(), &state->main_draw_context);
}

void scene_draw(scene* scene, const m4s top_matrix, draw_context* ctx) {
    for (u32 i = 0; i < scene->top_node_count; ++i) {
        node_draw(scene->top_nodes[i], top_matrix, ctx);
    }
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
    // TEMP: Test layout
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
    // TEMP: Test layout

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
        sizeof(VkDescriptorSet) * scene->state->image_count,
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
    scene->draws_buffer = etallocate(sizeof(buffer) * scene->state->image_count, MEMORY_TAG_SCENE);
    scene->scene_uniforms = etallocate(sizeof(buffer) * scene->state->image_count, MEMORY_TAG_SCENE);
    for (u32 i = 0; i < scene->state->image_count; ++i) {
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
            &scene->draws_buffer[i]);
        // TEMP: END
    }

    // Write buffers to DescriptorSets
    for (u32 i = 0; i < scene->state->image_count; ++i) {
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
            .buffer = scene->draws_buffer[i].handle,
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
    const char* vertex_path = "build/assets/shaders/bindless_mesh_mat.vert.spv";
    const char* fragment_path = "build/assets/shaders/bindless_mesh_mat.frag.spv";

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

    buffer_destroy(state, &scene->bindless_material_buffer);
    buffer_destroy(state, &scene->index_buffer);
    buffer_destroy(state, &scene->vertex_buffer);
    buffer_destroy(state, &scene->transform_buffer);

    vkDestroyPipeline(state->device.handle, scene->opaque_pipeline, state->allocator);
    vkDestroyPipeline(state->device.handle, scene->transparent_pipeline, state->allocator);
    vkDestroyPipelineLayout(state->device.handle, scene->pipeline_layout, state->allocator);

    for (u32 i = 0; i < state->image_count; ++i) {
        buffer_destroy(state, scene->scene_uniforms + i);
        buffer_destroy(state, scene->draws_buffer + i);
    }

    etfree(scene->scene_uniforms, sizeof(buffer) * state->image_count, MEMORY_TAG_SCENE); 
    etfree(scene->draws_buffer, sizeof(buffer) * state->image_count, MEMORY_TAG_SCENE);

    vkDestroyDescriptorPool(
        state->device.handle,
        scene->bindless_pool,
        state->allocator);
    etfree(
        scene->scene_sets,
        sizeof(VkDescriptorSet) * scene->state->image_count,
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

void scene_draw_bindless(scene* scene) {
    surface_2* surfaces = scene->surfaces;
    mesh_2* meshes = scene->meshes;
    object* objects = scene->objects;

    u64 object_count = dynarray_length(scene->objects);
    for (u64 i = 0; i < object_count; ++i) {
        mesh_2 mesh = meshes[objects[i].mesh_index];
        for (u32 j = mesh.start_surface; j < mesh.start_surface + mesh.surface_count; ++j) {
            draw_command draw = {
                .draw = {
                    .firstIndex = surfaces[j].start_index,
                    .instanceCount = 1,
                    .indexCount = surfaces[j].index_count,
                    .vertexOffset = 0,
                    .firstInstance = 0,
                },
                .material_instance_id = surfaces[j].material.pass,
                .transform_id = objects[i].transform_index,
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

b8 scene_render_bindless(scene* scene) {
    renderer_state* state = scene->state;

    // Copy scene data to the current frames uniform buffer
    void* mapped_uniform;
    vkMapMemory(
        state->device.handle,
        scene->scene_uniforms[state->frame_index].memory,
        /* Offset: */ 0,
        VK_WHOLE_SIZE,
        /* Flags: */ 0,
        &mapped_uniform);
    etcopy_memory(mapped_uniform, &scene->data, sizeof(scene_data));
    vkUnmapMemory(state->device.handle, scene->scene_uniforms[state->frame_index].memory);

    // Copy created draws over to the draws buffer for indirect drawing
    // TODO: Stop draw count from going over the temporary max draw count value
    u64 op_draws_size = sizeof(draw_command) * (scene->op_draws_count = dynarray_length(scene->opaque_draws));
    u64 tp_draws_size = sizeof(draw_command) * (scene->tp_draws_count = dynarray_length(scene->transparent_draws));
    // TODO: END

    void* mapped_draws;
    vkMapMemory(
        state->device.handle,
        scene->draws_buffer[state->frame_index].memory,
        /* Offset: */ 0,
        op_draws_size + tp_draws_size,
        /* Flags: */ 0,
        &mapped_draws);
    etcopy_memory(mapped_draws, scene->opaque_draws, op_draws_size);
    etcopy_memory((u8*)mapped_draws + op_draws_size, scene->opaque_draws, tp_draws_size);
    vkUnmapMemory(state->device.handle, scene->draws_buffer[state->frame_index].memory);

    // Clear draws after copied to draws buffer
    dynarray_clear(scene->opaque_draws);
    dynarray_clear(scene->transparent_draws);
    return false;
}

b8 scene_material_set_instance_bindless(scene* scene, material_pass pass_type, struct bindless_material_resources* resources) {
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

material_2 scene_material_get_instance_bindless(scene* scene, material_id id) {
    if (id.instance_id > scene->material_count || scene->materials[id.instance_id].id.instance_id == INVALID_ID) {
        ETERROR("Material not present. Returning invalid material.");
        // TODO: Create a default material to return a material_2 struct for
        return (material_2){.id = {.blueprint_id = INVALID_ID, .instance_id = INVALID_ID}, .pass = MATERIAL_PASS_OTHER};
    }
    return scene->materials[id.instance_id];
}

void scene_image_set_bindless(scene* scene, u32 img_id, u32 sampler_id) {
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