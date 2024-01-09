#pragma once

#include "renderer/src/vk_types.h"
#include "renderer/src/renderer.h"

b8 initialize_swapchain(renderer_state* state);

void shutdown_swapchain(renderer_state* state);

b8 recreate_swapchain(renderer_state* state);