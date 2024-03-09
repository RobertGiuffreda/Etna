#pragma once
#include "resource_types.h"
#include "renderer/src/vk_types.h"
#include "resources/resource_manager.h"

struct image_manager {
    renderer_state* state;
    
    image images[MAX_IMAGE_COUNT];
    u32 image_count;
};