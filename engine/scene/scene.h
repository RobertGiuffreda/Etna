#pragma once
#include "defines.h"
#include "math/math_types.h"
#include "renderer/renderer_types.h"

typedef struct scene scene;

b8 scene_initalize(scene** scn, renderer_state* state);

void scene_update(scene* scene);

void scene_shutdown(scene* scene);