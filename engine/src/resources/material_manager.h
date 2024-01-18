#pragma once

#include "resource_types.h"

typedef struct material_manager material_manager;

b8 material_manager_initialize(material_manager** manager, struct renderer_state* state);

void material_manager_shutdown(material_manager* manager);

material* material_get(material_manager* manager, u32 id);