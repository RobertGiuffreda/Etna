#pragma once

#include "defines.h"

#define INVALID_ID 0xFFFFFFFF

/** NOTE:
 * Functionality of the xxxxx_manager objects is currently
 * very limited. 
 * They essentially function as linear allocators for each 
 * kind of object they manage. So assets cannot be removed or
 * released from the managers.
 * 
 * Once I get a better handle of what will be required of the managers,
 * they will be modified.
 * 
 * Mesh manager currently allocates as 3 memory allocations(staging buffer, index buffer, vertex buffer)
 * for each mesh before the wait operation is called and the staging buffers are destroyed.
 * A staging buffer system using the dedicated transfer queue and suballocation of memory should be
 * implemented to minimize memory allocations as the spec only allows 4096
 */

// TODO: xxxxx_manager_load vs xxxxx_manager_submit

typedef struct mesh_1 mesh_1;
typedef struct image image;
typedef struct material_1 material_1;
typedef struct material_resource material_resource;

typedef struct mesh_manager mesh_manager;
typedef struct image_manager image_manager;
typedef struct material_manager material_manager;

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

typedef struct object {
    u32 surface_id;
    u32 transform_id;
} object;

// TEMP: Until refactor to move material from surface and add culling bounds
typedef struct gpu_obj {
    // TEMP: Above
    u32 start_index;
    u32 index_count;
    // TEMP: END

    u32 mat_inst_id;
    u32 transform_id;
} gpu_obj;
// TEMP: END