#pragma once
#include "defines.h"

#define CGLM_FORCE_DEPTH_ZERO_TO_ONE
#include <cglm/struct.h>

typedef vec2s v2s;
typedef vec3s v3s;
typedef vec4s v4s;

typedef versors quat;   // quaternion

typedef ivec2s iv2s;
typedef ivec3s iv3s;
typedef ivec4s iv4s;

typedef mat3s m3s;
typedef mat4s m4s;

typedef struct vert {
    f32 pos[3];
    f32 uv[4];
    f32 normal[3];
    f32 color[4];
} vert;

typedef struct vertex {
    v3s position;
    f32 uv_x;
    v3s normal;
    f32 uv_y;
    v4s color;
} vertex;

typedef struct skin_vertex {
    vertex vertex;
    iv4s joints;
    v4s weights;
} skin_vertex;
