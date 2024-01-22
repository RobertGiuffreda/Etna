#pragma once

#include "resource_types.h"

typedef struct material_manager material_manager;

b8 material_manager_initialize(material_manager** manager, struct renderer_state* state);

void material_manager_shutdown(material_manager* manager);

material* material_get(material_manager* manager, u32 id);

struct material_resources;
typedef struct material_config {
    char* name;
    material_pass pass_type;
    struct material_resources* resources;
} material_config;

b8 material_submit_ref(
    material_manager* manager,
    material_config* config,
    material** out_material_ref);

b8 material_submit(
    material_manager* manager,
    material_config* config);