#pragma once

#include "defines.h"

#define INVALID_ID 0xFFFFFFFF

// Texture manager??, descriptor set management of Image + Sampler
typedef struct image image;
typedef struct image_manager image_manager;

typedef enum material_pass {
    MATERIAL_PASS_MAIN_COLOR,
    MATERIAL_PASS_TRANSPARENT,
    MATERIAL_PASS_OTHER
} material_pass;

typedef struct material_id {
    u32 blueprint_id;
    u32 instance_id;
} material_id;

typedef struct material {
    material_pass pass;
    material_id id;
} material;
 
typedef struct surface {
    u32 start_index;
    u32 index_count;

    material material;
} surface;

typedef struct mesh {
    u32 vertex_offset;
    u32 transform_offset;
    u32 instance_count;

    u32 start_surface;
    u32 surface_count;
} mesh;

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
