#pragma once
#include "defines.h"
#include "math/math_types.h"

#define INVALID_ID 0xFFFFFFFF

// Texture manager??, descriptor set management of Image + Sampler
typedef struct image image;
typedef struct image_manager image_manager;
 
typedef struct mat_id {
    u32 pipe_id;
    u32 inst_id;
} mat_id;

// NOTE: Each one describes a different pipeline object
typedef enum mat_pipe_type {
    MAT_PIPE_METAL_ROUGH,
    MAT_PIPE_METAL_ROUGH_TRANSPARENT,
    MAT_PIPE_MAX,
} mat_pipe_type;

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

// TODO: Add bounds for culling
typedef struct geometry {
    u32 start_index;
    u32 index_count;
    i32 vertex_offset;
    f32 radius;
    v4s origin;
    v4s extent;
} geometry;
// TODO: END

typedef struct object {
    u32 pso_id;
    u32 mat_id;
    u32 geo_id;
    u32 transform_id;
} object;
