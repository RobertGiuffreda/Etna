#include "scene.h"
#include "scene/scene_private.h"

#include "containers/dynarray.h"

#include "core/etmemory.h"
#include "core/etstring.h"
#include "core/logger.h"

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
#include "resources/loaders/gltfloader.h"
// TEMP: END

/** NOTE: Implementation details
 * Currently the scene descriptor set is hardcoded as a singular
 * uniform buffer that matches struct gpu_scene_data.
 */

b8 scene_initalize(scene* scene, struct renderer_state* state) {
    // NOTE: Will be implemented when serialization is 
    // implemented and we read a scene config file
    return true;
}

// TEMP: Scene parameters here are currently hard-coded for an imported gltf file.
// Eventually the material buffers will be handled by the material manager
// The samplers will have some kind of system as well to manage them
void scene_shutdown(scene* scene) {
    renderer_state* state = scene->state;

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

    mesh_manager_shutdown(scene->mesh_bank);

    buffer_destroy(state, &scene->material_buffer);
    material_manager_shutdown(scene->material_bank);
    image_manager_shutdown(scene->image_bank);

    for (u32 i = 0; i < scene->sampler_count; ++i)
        vkDestroySampler(state->device.handle, scene->samplers[i], state->allocator);
    etfree(scene->samplers, sizeof(VkSampler) * scene->sampler_count, MEMORY_TAG_SCENE);

    camera_destroy(&scene->cam);

    descriptor_set_allocator_growable_shutdown(&scene->mat_ds_allocator, state);

    // for (u32 i = 0; i < state->image_count; ++i) {
    //     descriptor_set_allocator_growable_shutdown(&scene->ds_allocators[i], state);
    //     buffer_destroy(state, &scene->scene_data_buffers[i]);
    // }
    // etfree(scene->ds_allocators);
    // etfree(scene->scene_data_buffers);

    str_duplicate_free(scene->name);
    
    scene->state = 0;
}

void scene_update(scene* scene) {
    renderer_state* state = scene->state;

    camera_update(&scene->cam);

    m4s view = camera_get_view_matrix(&scene->cam);

    // TODO: Camera should store near and far values & perspective matrix
    // TODO: Scene should register its camera with the event system, 
    m4s project = glms_perspective(
        /* fovy */ glm_rad(70.f),
        ((f32)state->window_extent.width/(f32)state->window_extent.height), 
        /* near-z: */ 10000.f,
        /* far-z: */ 0.1f);

    // invert the Y direction on projection matrix so that we are more similar
    // to opengl and gltf axis
    project.raw[1][1] *= -1;

    m4s vp = glms_mat4_mul(project, view);
    scene->data.view = view;
    scene->data.proj = project;
    scene->data.viewproj = vp;

    v4s a_color = { .raw = {.1f, .1f, .1f, .1f}};
    v4s l_color = { .raw = {1.f, 1.f, 1.f, 1.f}};

    // Manipulate this vector
    v4s l_pos = glms_vec4(scene->cam.position, 1.0f);
    l_pos.y += 2.0f;
    
    scene->data.ambient_color = a_color;
    scene->data.light_color = l_color;
    scene->data.light_position = l_pos;
    
    scene->data.view_pos = glms_vec4(scene->cam.position, 1.0f);
}

void scene_draw(struct scene* scene, const m4s top_matrix, struct draw_context* ctx) {
    for (u32 i = 0; i < scene->top_node_count; ++i) {
        node_draw(scene->top_nodes[i], top_matrix, ctx);
    }
}