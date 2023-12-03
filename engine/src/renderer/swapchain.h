#pragma once

#include "renderer_types.h"

b8 initialize_swapchain(renderer_state* state);

void shutdown_swapchain(renderer_state* state);

b8 recreate_swapchain(renderer_state* state);