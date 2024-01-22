#pragma once
#include "defines.h"
#include "math/math_types.h"

b8 scene_initalize(struct scene* scene, struct renderer_state* state);

void scene_update(struct scene* scene);

void scene_shutdown(struct scene* scene);

void scene_draw(struct scene* scene, const m4s top_matrix, struct draw_context* ctx);