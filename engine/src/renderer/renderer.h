#pragma once

#include "defines.h"

typedef struct renderer_state renderer_state;

b8 renderer_initialize(renderer_state** state, struct etwindow_state* window, const char* application_name);

b8 renderer_prepare_frame(renderer_state* state);

b8 renderer_draw_frame(renderer_state* state);

void renderer_shutdown(renderer_state* state);