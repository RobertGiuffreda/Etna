#pragma once
#include "resource_types.h"
#include "renderer/src/vk_types.h"

// TODO: Have a texture manager instead of an image manager
struct image_manager {
    renderer_state* state;
    
    image images[MAX_IMAGE_COUNT];
    u32 image_count;
};