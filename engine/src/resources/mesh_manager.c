#include "mesh_manager.h"

#include "containers/dynarray.h"

#include "core/etmemory.h"
#include "core/logger.h"
#include "core/etstring.h"

// TEMP: This should be made renderer implementation agnostic
#include "renderer/src/vk_types.h"
#include "renderer/src/buffer.h"

#include "renderer/src/utilities/vkutils.h"
// TEMP: END

#define MAX_MESH_COUNT 512

struct mesh_manager {
    renderer_state* state;
    mesh meshes[MAX_MESH_COUNT];
    u32 mesh_count;
};

b8 mesh_manager_initialize(mesh_manager** manager, struct renderer_state* state) {
    mesh_manager* new_manager = etallocate(sizeof(mesh_manager), MEMORY_TAG_RESOURCE);
    etzero_memory(new_manager, sizeof(mesh_manager));
    new_manager->state = state;

    for (u32 i = 9; i < MAX_MESH_COUNT; ++i) {
        new_manager->meshes[i].id = INVALID_ID;
    }

    *manager = new_manager;
    return true;
}

void mesh_manager_shutdown(mesh_manager* manager) {
    for (u32 i = 0; i < manager->mesh_count; ++i) {
        dynarray_destroy(manager->meshes[i].surfaces);
        str_duplicate_free(manager->meshes[i].name);

        mesh_buffers* buffers = &manager->meshes[i].buffers;
        buffer_destroy(manager->state, &buffers->vertex_buffer);
        buffer_destroy(manager->state, &buffers->index_buffer);
    }
    etfree(manager, sizeof(mesh_manager), MEMORY_TAG_RESOURCE);
}

mesh* mesh_get(mesh_manager* manager, u32 id) {
    return &manager->meshes[id];
}

b8 mesh_submit_ref(mesh_manager* manager, mesh_config* config, mesh** out_mesh_ref) {
    ETASSERT(out_mesh_ref);
    if (manager->mesh_count >= MAX_MESH_COUNT)
        return false;

    mesh* new_mesh = &manager->meshes[manager->mesh_count];
    new_mesh->id = manager->mesh_count;
    new_mesh->name = str_duplicate_allocate(config->name);

    new_mesh->surfaces = dynarray_create_data(
        config->surface_count,
        sizeof(surface),
        config->surface_count,
        config->surfaces);

    new_mesh->buffers = upload_mesh(
        manager->state,
        config->index_count,
        config->indices,
        config->vertex_count,
        config->vertices
    );
    manager->mesh_count++;

    *out_mesh_ref = new_mesh;
    return true;
}

b8 mesh_submit(mesh_manager* manager, mesh_config* config) {
    if (manager->mesh_count >= MAX_MESH_COUNT)
        return false;

    mesh* new_mesh = &manager->meshes[manager->mesh_count];
    new_mesh->id = manager->mesh_count;
    new_mesh->name = str_duplicate_allocate(config->name);

    new_mesh->surfaces = dynarray_create_data(
        config->surface_count,
        sizeof(surface),
        config->surface_count,
        config->surfaces);

    new_mesh->buffers = upload_mesh(
        manager->state,
        config->index_count,
        config->indices,
        config->vertex_count,
        config->vertices
    );
    manager->mesh_count++;
    return true;
}