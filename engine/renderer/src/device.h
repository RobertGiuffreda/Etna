#pragma once

#include "renderer/src/vk_types.h"

b8 device_create(renderer_state* state, device* out_device);

void device_destroy(renderer_state* state, device* device);