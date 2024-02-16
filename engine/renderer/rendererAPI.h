#pragma once

#include "defines.h"

#include "renderer/renderer_types.h"

struct etwindow_t;

b8 renderer_initialize(renderer_state** out_state, struct etwindow_t* window, const char* application_name);

void renderer_shutdown(renderer_state* state);

b8 renderer_prepare_frame(renderer_state* state);

b8 renderer_draw_frame(renderer_state* state);

void renderer_on_resize(renderer_state* state, i32 width, i32 height);