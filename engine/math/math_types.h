#pragma once

#include "defines.h"

#define CGLM_FORCE_DEPTH_ZERO_TO_ONE
#include <cglm/struct.h>

typedef vec2s v2s;
typedef vec3s v3s;
typedef vec4s v4s;

typedef mat3s m3s;
typedef mat4s m4s;

typedef struct vertex {
    v3s position;
    f32 uv_x;
    v3s normal;
    f32 uv_y;
    v4s color;
} vertex;

typedef struct vertex2d {
    v2s position;
    v2s uv;
    v4s color;
} vertex2d;