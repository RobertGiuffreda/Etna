#pragma once
#include "resource_types.h"

#include "renderer/src/vk_types.h"
#include "renderer/src/renderer.h"

#define MAX_MATERIAL_COUNT 256
#define MAX_IMAGE_COUNT 128

// TODO: typedef id types for each resource

struct image_manager {
    renderer_state* state;
    
    image images[MAX_IMAGE_COUNT];
    u32 image_count;
};