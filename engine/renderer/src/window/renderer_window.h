#pragma once
#include "renderer/src/renderer.h"

struct etwindow_t;

b8 window_create_vulkan_surface(renderer_state* renderer_state, struct etwindow_t* window);

const char** window_get_required_extension_names(i32* count);