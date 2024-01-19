#pragma once
#include "defines.h"
#include "math/math_types.h"
#include "resources/resource_types.h"
#include "core/camera.h"

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

    struct scene_renderer* data;
    struct renderer_state* state;
} scene;

b8 scene_initalize(scene* scene, struct renderer_state* state);

void scene_update(scene* scene);

b8 scene_load_from_gltf(scene* scene, const char* path);