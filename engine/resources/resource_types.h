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

typedef struct mesh mesh;
typedef struct image image;
typedef struct material material;
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

typedef struct material_2 {
    material_pass pass;
    material_id id;
} material_2;

typedef struct surface_2 {
    u32 start_index;
    u32 index_count;

    material_2 material;
} surface_2;

typedef struct mesh_2 {
    u32 start_surface;
    u32 surface_count;
} mesh_2;

typedef struct object {
    u32 mesh_index;
    u32 transform_index;
} object;
