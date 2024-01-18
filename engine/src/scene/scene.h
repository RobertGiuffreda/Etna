#pragma once
#include "defines.h"
#include "resources/resource_types.h"

#include "core/camera.h"

#include "node.h"

typedef struct scene {
    mesh** meshes;
    image** images;
    material** materials;

    node** top_nodes;

    camera cam;

    // TEMP: Improve lighting scheme
    v4s ambient_col;
    v4s light_dir;
    v4s light_col;
    // TEMP: END
} scene;

