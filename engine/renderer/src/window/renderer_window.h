#pragma once
#include "renderer/src/renderer.h"

struct etwindow_state;

b8 window_create_vulkan_surface(renderer_state* renderer_state, struct etwindow_state* window_state);

const char** window_get_required_extension_names(i32* count);