#pragma once
#include "defines.h"
#include "math/math_types.h"
#include "renderer/src/vk_types.h"
#include "resources/material_refactor.h"

#define MAX_DRAW_COMMANDS 8192
#define MAX_OBJECTS 8192

typedef struct blinn_mr_ubo {
    v4s color;
    u32 color_id;
    f32 metalness;
    f32 roughness;
    u32 mr_id;
} blinn_mr_ubo;

typedef struct solid_ubo {
    v4s color;
} solid_ubo;