#pragma once
#include "defines.h"
#include "math/math_types.h"
#include "renderer/renderer_types.h"

typedef struct scene scene;

b8 scene_initalize(scene* scene, renderer_state* state);

void scene_update(scene* scene);

void scene_shutdown(scene* scene);

void scene_draw(scene* scene, const m4s top_matrix, draw_context* ctx);
