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
    struct mesh_manager* mesh_bank;
    struct image_manager* image_bank;
    struct material_manager* material_bank;

    struct node** top_nodes;

    camera cam;

    // TEMP: Improve lighting scheme
    v4s ambient_col;
    v4s light_dir;
    v4s light_col;
    // TEMP: END

    struct renderer_state* state;
} scene;

b8 scene_load_from_gltf(scene* scene, struct renderer_state* state, const char* path);