#pragma once
#include "platform/etwindow.h"
#include "renderer/renderer_types.h"

b8 renderer_window_init(renderer_state* renderer_state, etwindow_t* window);

b8 window_create_vulkan_surface(renderer_state* renderer_state, etwindow_t* window);

const char** window_get_required_extension_names(i32* count);