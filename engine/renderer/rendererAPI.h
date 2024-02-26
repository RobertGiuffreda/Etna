#pragma once
#include "defines.h"
#include "platform/etwindow.h"
#include "renderer/renderer_types.h"

// NOTE: Pass null window for headless rendering.
typedef struct renderer_config {
    const char* engine_name;
    const char* app_name;
    etwindow_t* window;
} renderer_config;

b8 renderer_initialize(renderer_state** out_state, renderer_config config);

void renderer_shutdown(renderer_state* state);