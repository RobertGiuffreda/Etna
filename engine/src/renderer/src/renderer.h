#pragma once

#include "defines.h"

// TODO: This interface is the current RendererAPI for the engine. 
// This should be moved outside of the renderer src folder and contain
// Other renderer abstraction functions. Like renderer specific loading
// functions. Opaque types defined here 

typedef struct renderer_state renderer_state;

b8 renderer_initialize(renderer_state** state, struct etwindow_state* window, const char* application_name);

void renderer_shutdown(renderer_state* state);

void renderer_update_scene(renderer_state* state);

b8 renderer_draw_frame(renderer_state* state);

void renderer_on_resize(renderer_state* state, i32 width, i32 height);