#pragma once
#include "resource_types.h"
#include "renderer/renderer_types.h"

b8 mesh_manager_initialize(mesh_manager** manager, renderer_state* state);

void mesh_manager_shutdown(mesh_manager* manager);

mesh_1* mesh_manager_get(mesh_manager* manager, u32 id);

struct surface_1;
struct vertex;
typedef struct mesh_config {
    char* name;
    struct surface_1* surfaces;
    u32 surface_count;

    struct vertex* vertices;
    u32 vertex_count;
    u32* indices;
    u32 index_count;
} mesh_config;

b8 mesh_manager_submit_immediate(mesh_manager* manager, mesh_config* config);

b8 mesh_manager_submit(mesh_manager* manager, mesh_config* config);

// Waits for all meshes to finish uploading to the gpu
b8 mesh_manager_uploads_wait(mesh_manager* manager);