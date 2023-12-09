#pragma once

#include "renderer_types.h"

// TODO: Better isolate this library (spirv reflect) in the system to make
// changing it out to something else like SPIRV-Cross easier if it becomes
// depricated.
#include <spirv_reflect.h>

#define SPIRV_REFLECT_CHECK(expr) { ETASSERT((expr) == SPV_REFLECT_RESULT_SUCCESS); }

typedef struct shader {
    VkShaderModule module;
} shader;

b8 load_shader(const char* path, shader* out_shader);