#pragma once

#include "renderer/src/vk_types.h"

#include "renderer/src/shader.h"

typedef struct material_blueprint {
    shader vertex;
    shader fragment;

    
} material_blueprint;

b8 material_blueprint_create();