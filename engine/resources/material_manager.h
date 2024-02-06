#pragma once
#include "resource_types.h"
#include "renderer/renderer_types.h"

b8 material_manager_initialize(material_manager** manager, renderer_state* state);

void material_manager_shutdown(material_manager* manager);

material* material_manager_get(material_manager* manager, u32 id);

typedef struct material_config {
    char* name;
    material_pass pass_type;
    struct material_resources* resources;
} material_config;

b8 material_manager_submit(material_manager* manager, material_config* config);