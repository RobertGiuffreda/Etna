#pragma once
#include "renderer/src/vk_types.h"
#include "resources/resource_types.h"
#include "resources/material_refactor.h"

#define MAX_BUFFER_COUNT 1024
#define MAX_IMAGE_COUNT 512
#define MAX_MATERIAL_COUNT 512

typedef struct resource_manager {
    buffer buffers[MAX_BUFFER_COUNT];
    u32 buffer_count;
    image images[MAX_IMAGE_COUNT];
    u32 image_count;
    mat_pipe materials[MAT_PIPE_MAX];
} resource_manager;