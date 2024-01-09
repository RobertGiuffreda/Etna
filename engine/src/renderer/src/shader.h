#pragma once

#include "renderer/src/vk_types.h"
#include "renderer/src/renderer.h"

// TODO: Better isolate this library (spirv reflect) in the system to make
// changing it out to something else like SPIRV-Cross easier if it becomes
// depricated.
#include <spirv_reflect.h>

#define SPIRV_REFLECT_CHECK(expr) { ETASSERT((expr) == SPV_REFLECT_RESULT_SUCCESS); }

b8 load_shader(renderer_state* state, const char* path, shader* out_shader);

void unload_shader(renderer_state* state, shader* shader);