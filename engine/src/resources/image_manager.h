#pragma once

#include "resource_types.h"

typedef struct image_manager image_manager;

b8 image_manager_initialize(image_manager** manager, struct renderer_state* state);

void image_manager_shutdown(image_manager* manager);

image* image_get(image_manager* manager, u32 id);