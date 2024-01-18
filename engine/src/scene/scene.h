#pragma once
#include "defines.h"
#include "resources/resource_types.h"

#include "core/camera.h"

// Forward declarations
struct renderer_state;
struct mesh_manager;
struct image_manager;
struct material_manager;
struct node;

typedef struct scene {
    mesh_manager* mesh_bank;
    image_manager* image_bank;
    material_manager* material_bank;

    node** top_nodes;

    camera cam;

    // TEMP: Improve lighting scheme
    v4s ambient_col;
    v4s light_dir;
    v4s light_col;
    // TEMP: END

    renderer_state* state;
} scene;

b8 scene_load_from_gltf(scene* scene, renderer_state* state, const char* path);

