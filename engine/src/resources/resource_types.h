#pragma once

#include "defines.h"

#define INVALID_ID 0xFFFFFFFF

/** NOTE:
 * Functionality of the xxxx_manager objects is currently
 * very limited. They will be built upon.
 * 
 * Currently assets cannot be removed or released from the managers.
 * Once I get a better handle of what will be required of the managers,
 * they will be built upon.
 * 
 * NOTE: Managers currently untested
 */

// TODO: Invalid id value

typedef struct mesh mesh;
typedef struct image image;
typedef struct material material;