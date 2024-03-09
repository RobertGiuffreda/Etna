#pragma once

#include "defines.h"

#define INVALID_ID 0xFFFFFFFF

// Texture manager??, descriptor set management of Image + Sampler
typedef struct image image;
typedef struct image_manager image_manager;
 
// NOTE: GPU structures that are constructed on scene_render_init after various files have been imported
// This system will probobly change when UUIDs and a custom file format gets added

// TODO: Add bounds for culling
typedef struct geometry {
    u32 start_index;
    u32 index_count;
    i32 vertex_offset;  // TODO: Use this variable
} geometry;
// TODO: END

typedef struct object {
    u32 pso_id;
    u32 mat_id;
    u32 geo_id;
    u32 transform_id;
} object;
