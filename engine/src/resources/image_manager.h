#pragma once
#include "resource_types.h"
#include "renderer/renderer_types.h"

typedef struct image_manager image_manager;

b8 image_manager_initialize(image_manager** manager, renderer_state* state);

void image_manager_shutdown(image_manager* manager);

// TEMP: Function to increment the image count to skip over images
// that do not load properly leaving the error_image reference 
void image_manager_increment(image_manager* manager);
// TEMP: END

image* image_manager_get(image_manager* manager, u32 id);

// TODO: Configurable format here
typedef struct image_config {
    char* name;
    u32 width;
    u32 height;
    void* data;
} image_config;

b8 image2D_submit(image_manager* manager, image_config* config);