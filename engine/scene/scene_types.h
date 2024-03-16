#pragma once
#include "defines.h"
#include "math/math_types.h"
#include "renderer/src/vk_types.h"
#include "resources/material.h"

#define MAX_DRAW_COMMANDS 8192
#define MAX_OBJECTS 8192

// TODO: Read from shader reflection data.
// NOTE: Spirv-reflect is dereferencing a null pointer on me at the moment
typedef enum scene_set_bindings {
    SCENE_SET_FRAME_UNIFORMS_BINDING = 0,
    SCENE_SET_DRAW_COUNTS_BINDING,
    SCENE_SET_DRAW_BUFFERS_BINDING,
    SCENE_SET_OBJECTS_BINDING,
    SCENE_SET_GEOMETRIES_BINDING,
    SCENE_SET_VERTICES_BINDING,
    SCENE_SET_TRANSFORMS_BINDING,
    SCENE_SET_TEXTURES_BINDING,
    SCENE_SET_BINDING_MAX,
} scene_set_bindings;
