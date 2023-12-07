#pragma once

#include "renderer/renderer_types.h"

// TEMP: This entire header and corresponding c file.
// This will be chnaged to make something more configurable
// When SPIR-V reflection is implemented. 

typedef struct compute_pipeline_config {
    const char* shader;
} compute_pipeline_config;

b8 compute_pipeline_create(renderer_state* state, compute_pipeline_config config, compute_pipeline* out_pipeline);

void compute_pipeline_destroy(renderer_state* state, compute_pipeline* pipeline);