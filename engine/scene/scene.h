#pragma once
#include "defines.h"
#include "math/math_types.h"
#include "renderer/renderer_types.h"

typedef struct scene scene;

b8 scene_initalize(scene** scn, renderer_state* state);

void scene_update(scene* scene, f64 dt);

b8 scene_frame_begin(scene* scene, renderer_state* state);

b8 scene_frame_end(scene* scene, renderer_state* state);

b8 scene_render(scene* scene);

void scene_shutdown(scene* scene);

void scene_texture_set(scene* scene, u32 img_id, u32 sampler_id);