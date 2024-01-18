#pragma once

#include "resource_types.h"

typedef struct mesh_manager mesh_manager;

b8 mesh_manager_initialize(mesh_manager** manager, struct renderer_state* state);

void mesh_manager_shutdown(mesh_manager* manager);

mesh* mesh_get(mesh_manager* manager, u32 id);