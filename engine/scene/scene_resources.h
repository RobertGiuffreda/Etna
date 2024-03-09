#pragma once
#include "defines.h"

/** TEMP: This file
 * This file is temporary until resources interaction with the scene is figured out
 * 
 */
typedef struct surface {
    u32 start_index;
    u32 index_count;

    mat_id material;
} surface;

typedef struct mesh {
    u32 vertex_offset;
    u32 transform_offset;
    u32 instance_count;

    u32 start_surface;
    u32 surface_count;
} mesh;

void scene_texture_set(scene* scene, u32 img_id, u32 sampler_id);