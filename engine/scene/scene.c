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

// TEMP: Make this renderer implementation agnostic
#include "renderer/src/vk_types.h"
#include "renderer/src/renderer.h"
#include "renderer/src/buffer.h"
#include "renderer/src/descriptor.h"
#include "renderer/src/shader.h"
#include "renderer/src/renderables.h"
#include "resources/importers/gltfimporter.h"
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

    // TEMP: Bindless
    scene_init_bindless(new_scene);
    // TEMP: END

    // pool_size_ratio ps_ratios[] = {
    //     { .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .ratio = 3},
    //     { .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .ratio = 3},
    //     { .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .ratio = 3},
    //     { .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .ratio = 3}};
    // new_scene->ds_allocators = etallocate(sizeof(ds_allocator) * state->image_count, MEMORY_TAG_SCENE);
    // new_scene->scene_data_buffers = etallocate(sizeof(buffer) * state->image_count, MEMORY_TAG_SCENE);
    // for (u32 i = 0; i < state->image_count; ++i) {
    //     buffer_create(
    //         state,
    //         sizeof(scene_data),
    //         VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
    //         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    //         &new_scene->scene_data_buffers[i]);
    //     descriptor_set_allocator_initialize(
    //         &new_scene->ds_allocators[i],
    //         /* Initial Sets: */ 1000,
    //         /* pool size count: */ 4,
    //         ps_ratios,
    //         state);
    // }

    camera_create(&new_scene->cam);
    new_scene->cam.position = (v3s){.raw = {0.0f, 0.0f, 5.0f}};

    // NOTE: This will be passed a config when serialization is implemented
    v4s a_color = { .raw = {.1f, .1f, .1f, 3.f}};
    v4s l_color = { .raw = {1.f, 1.f, 1.f, 50.f}};
    
    new_scene->data.ambient_color = a_color;
    new_scene->data.light_color = l_color;

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

    // for (u32 i = 0; i < state->image_count; ++i) {
    //     descriptor_set_allocator_shutdown(&scene->ds_allocators[i], state);
    //     buffer_destroy(state, &scene->scene_data_buffers[i]);
    // }
    // etfree(scene->ds_allocators);
    // etfree(scene->scene_data_buffers);

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
    scene->opaque_draws = dynarray_create(1, sizeof(draw_command));
    scene->transparent_draws = dynarray_create(1, sizeof(draw_command));

    

    // Set 0: Scene set layout (engine specific). The set itself will be allocated on the fly
    dsl_builder scene_builder = descriptor_set_layout_builder_create();
    descriptor_set_layout_builder_add_binding(
        &scene_builder,
        /* Binding: */ 0,
        /* Count: */ 1,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    descriptor_set_layout_builder_add_binding(
        &scene_builder,
        /* Binding: */ 1,
        /* Count: */ 1,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    scene->scene_layout = descriptor_set_layout_builder_build(
        &scene_builder,
        scene->state);
    descriptor_set_layout_builder_destroy(&scene_builder);

    // TODO: Read from shader reflection data
    // Set 1: Material layout (material specific). Arrays of each element
    dsl_builder mat_builder = descriptor_set_layout_builder_create();
    descriptor_set_layout_builder_add_binding(
        &mat_builder,
        /* Binding: */ 0,
        /* Count: */ MAX_MATERIAL_COUNT,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    descriptor_set_layout_builder_add_binding(
        &mat_builder,
        /* Binding: */ 1,
        /* Count: */ MAX_MATERIAL_COUNT,
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    descriptor_set_layout_builder_add_binding(
        &mat_builder,
        /* Binding: */ 2,
        /* Count: */ MAX_MATERIAL_COUNT,
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    descriptor_set_layout_builder_destroy(&mat_builder);

    scene->material_count = 0;
    for (u32 i = 0; i < MAX_MATERIAL_COUNT; ++i) {
        scene->materials[i] = (material_2){.id = {.blueprint_id = INVALID_ID, .instance_id = INVALID_ID}, .pass = MATERIAL_PASS_OTHER};
    }
}

void scene_shutdown_bindless(scene* scene) {
    renderer_state* state = scene->state;
    dynarray_destroy(scene->objects);
    dynarray_destroy(scene->transforms);
    dynarray_destroy(scene->meshes);
    dynarray_destroy(scene->surfaces);

    buffer_destroy(state, &scene->index_buffer);
    buffer_destroy(state, &scene->vertex_buffer);
    buffer_destroy(state, &scene->transform_buffer);

    vkDestroyDescriptorSetLayout(
        state->device.handle,
        scene->scene_layout,
        state->allocator);

    dynarray_destroy(scene->transparent_draws);
    dynarray_destroy(scene->opaque_draws);
}

void scene_draw_bindless(scene* scene) {
    dynarray_clear(scene->opaque_draws);
    dynarray_clear(scene->transparent_draws);

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

void scene_material_set_instance_bindless(scene* scene, material_pass pass_type, struct GLTF_MR_material_resources* resources) {
    // TODO: Update the material set based on the amount of material instances
}

material_2 scene_material_get_instance_bindless(scene* scene, material_id id) {
    if (id.instance_id > scene->material_count || scene->materials[id.instance_id].id.instance_id == INVALID_ID) {
        ETERROR("Material not present. Returning invalid material.");
        // TODO: Create a default material to return a material_2 struct for
        return (material_2){.id = {.blueprint_id = INVALID_ID, .instance_id = INVALID_ID}, .pass = MATERIAL_PASS_OTHER};
    }
    return scene->materials[id.instance_id];
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