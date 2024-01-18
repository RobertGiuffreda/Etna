#include "material_manager.h"

#include "containers/dynarray.h"

#include "core/etmemory.h"
#include "core/logger.h"
#include "core/etstring.h"

// TEMP: This should be made renderer implementation agnostic
#include "renderer/src/vk_types.h"
#include "renderer/src/descriptor.h"
#include "renderer/src/GLTFMetallic_Roughness.h"
// TEMP: END

/** NOTE:
 * This currently only works for the material blueprint:
 * GLTF_MR. TODO: Extend to handle different material blueprints
 * 
 * Eventually load & serialize blueprints
 */

#define MAX_MATERIAL_COUNT 128

struct material_manager {
    renderer_state* state;
    ds_allocator_growable ds_allocator;
    GLTF_MR blueprint;
    
    material materials[MAX_MATERIAL_COUNT];
    u32 material_count;
};

b8 material_manager_initialize(material_manager** manager, struct renderer_state* state) {
    material_manager* new_manager = etallocate(sizeof(material_manager), MEMORY_TAG_RESOURCE);
    etzero_memory(new_manager, sizeof(material_manager));
    new_manager->state = state;

    pool_size_ratio ratios[] = {
        {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .ratio = 3},
        {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .ratio = 3},
        {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .ratio = 1}
    };
    pool_size_ratio* dynarray_ratios = 
        dynarray_create_data(3, sizeof(pool_size_ratio), 3, ratios);
    descriptor_set_allocator_growable_initialize(
        &new_manager->ds_allocator,
        MAX_MATERIAL_COUNT,
        dynarray_ratios,
        state);
    dynarray_destroy(dynarray_ratios);
    GLTF_MR_build_pipelines(&new_manager->blueprint, state);

    for (u32 i = 0; i < MAX_MATERIAL_COUNT; ++i) {
        new_manager->materials[i].id = INVALID_ID;
    }

    *manager = new_manager;
    return true;
}

void material_manager_shutdown(material_manager* manager) {
    GLTF_MR_destroy_pipelines(&manager->blueprint, manager->state);
    descriptor_set_allocator_growable_shutdown(&manager->ds_allocator, manager->state);
    for (u32 i = 0; i < manager->material_count; ++i) {
        str_duplicate_free(manager->materials[i].name);
    }
    etfree(manager, sizeof(material_manager), MEMORY_TAG_RESOURCE);
}

material* material_get(material_manager* manager, u32 id) {
    return &manager->materials[id];
}