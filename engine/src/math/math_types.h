#pragma once

#include "defines.h"

#include <cglm/struct.h>

typedef vec2s v2s;
typedef vec3s v3s;
typedef vec4s v4s;

typedef mat3s m3;
typedef mat4s m4;

typedef struct vertex3d {
    v3s position;
    v2s texture;
} vertex3d;