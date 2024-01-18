#pragma once

#include "renderer_types.h"

b8 renderer_initialize(renderer_state** state, struct etwindow_state* window, const char* application_name);

void renderer_shutdown(renderer_state* state);

void renderer_update_scene(renderer_state* state);

b8 renderer_draw_frame(renderer_state* state);

void renderer_on_resize(renderer_state* state, i32 width, i32 height);