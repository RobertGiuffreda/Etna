#include "scene.h"

#include "resources/mesh_manager.h"
#include "resources/image_manager.h"
#include "resources/material_manager.h"

// TEMP: Make this renderer implementation agnostic
#include "renderer/src/vk_types.h"
#include "renderer/src/renderables.h"
#include "resources/loaders/gltfloader.h"
#include "renderer/src/shader.h"
// TEMP: END

/** NOTE: Implementation details
 * Currently the scene descriptor set is hardcoded as a singular
 * uniform buffer that matches struct gpu_scene_data.
 * TODO: Support for variable scene descriptor set data through
 * "shader.h"'s shader_descriptor_set_layout structure.
 */

struct scene_renderer {
    VkDescriptorSetLayout layout;
    // Scene data to be sent to the GPU
    gpu_scene_data data;

    // For items that need to modified per frame
    ds_allocator_growable* ds_allocators;
    buffer* scene_data_buffers;
};

b8 scene_initalize(scene* scene, struct renderer_state* state) {
    scene->state = state;

    mesh_manager_initialize(&scene->mesh_bank, state);
    image_manager_initialize(&scene->image_bank, state);
    material_manager_initialize(&scene->material_bank, state);

    camera_create(&scene->cam);
    return true;
}

void scene_shutdown(scene* scene) {
    camera_destroy(&scene->cam);

    material_manager_shutdown(scene->material_bank);
    image_manager_shutdown(scene->image_bank);
    mesh_manager_shutdown(scene->mesh_bank);

    scene->state = 0;
}

void scene_update(scene* scene) {
    camera_update(&scene->cam);
}

b8 scene_load_from_gltf(scene* scene, const char* path);