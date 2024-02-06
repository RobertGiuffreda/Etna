#pragma once

/** TEMP: 
 * This file is a temporary measure to avoid exposing vulkan to 
 * the rest of the engine before I have some manager data structures
 * fully defined and implemented in scene
 */

#include "defines.h"
#include "core/camera.h"
#include "math/math_types.h"
#include "renderer/src/vk_types.h"
#include "renderer/src/renderables.h"
#include "resources/resource_types.h"

struct mesh_manager;
struct image_manager;
struct material_manager;

typedef struct scene {
    char* name;

    // TODO: Move managers to some project representation that contains scenes
    struct mesh_manager* mesh_bank;
    struct image_manager* image_bank;
    struct material_manager* material_bank;
    // TODO: END

    node** top_nodes;
    u32 top_node_count;

    camera cam;
    scene_data data;

    // TEMP: Leftovers from loaded_gltf not refactored out yet
    mesh_node* mesh_nodes;
    u32 mesh_node_count;
    node* nodes;
    u32 node_count;

    u32 sampler_count;
    VkSampler* samplers;

    buffer material_buffer;
    // TEMP: END

    // For items that need to modified per frame
    // ds_allocator* ds_allocators;
    // buffer* scene_data_buffers;

    renderer_state* state;
} scene;
