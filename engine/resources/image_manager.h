#pragma once
#include "resource_types.h"
#include "renderer/renderer_types.h"

b8 image_manager_initialize(image_manager** manager, renderer_state* state);

void image_manager_shutdown(image_manager* manager);

image* image_manager_get(image_manager* manager, u32 id);

typedef struct image2D_config {
    char* name;
    u32 width;
    u32 height;
    void* data;
} image2D_config;

u32 image2D_submit(image_manager* manager, image2D_config* config);
