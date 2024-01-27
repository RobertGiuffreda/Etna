#pragma once

#include "defines.h"

#define INVALID_ID 0xFFFFFFFF

/** NOTE:
 * Functionality of the xxxx_manager objects is currently
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

typedef struct mesh mesh;
typedef struct image image;
typedef struct material material;

typedef enum material_pass {
    MATERIAL_PASS_MAIN_COLOR,
    MATERIAL_PASS_TRANSPARENT,
    MATERIAL_PASS_OTHER
} material_pass;
