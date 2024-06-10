#pragma once
#include "defines.h"
#include "math/math_types.h"

#define INVALID_ID 0xFFFFFFFF

// TODO: Remove image manager, replace with 
// something to track images & textures
typedef struct image image;
typedef struct image_manager image_manager;
// TODO: END

#define MAX_BUFFER_COUNT 1024
#define MAX_IMAGE_COUNT 512
#define MAX_TEXTURE_COUNT 1024
#define MAX_MATERIAL_COUNT 512

/** TODO:
 * Rename Geometry to submesh??
 * Rename Object to instance??
 */ 

// TODO: _id --> _index
typedef struct mat_id {
    u32 pipe_id;
    u32 inst_id;
} mat_id;
// TODO: END

typedef struct geometry {
    u32 start_index;
    u32 index_count;
    u32 padd;
    f32 radius;
    v4s origin;
    v4s extent;
} geometry;

// TODO: _id --> _index
typedef struct object {
    u32 pipe_id;            // Pipeline shader object index
    u32 mat_id;             // Material instance index
    u32 geo_id;             // Geometry index
    i32 vertex_offset;      // Vertex offset
    u32 transform_id;       // Transform index
} object;
// TODO: END
