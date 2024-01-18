#include "image_manager.h"

#include "core/etmemory.h"
#include "core/logger.h"

// TEMP: This should be made renderer implementation agnostic
#include "renderer/src/vk_types.h"
#include "renderer/src/image.h"
// TEMP: END

#define MAX_IMAGE_COUNT 128

struct image_manager {
    renderer_state* state;
    image images[MAX_IMAGE_COUNT];
    u32 image_count;
};

b8 image_manager_initialize(image_manager** manager, struct renderer_state* state) {
    image_manager* new_manager = etallocate(sizeof(image_manager), MEMORY_TAG_RESOURCE);
    etzero_memory(new_manager, sizeof(image_manager));
    new_manager->state = state;

    for (u32 i = 0; i < MAX_IMAGE_COUNT; ++i) {
        new_manager->images[i].id = INVALID_ID;
    }

    *manager = new_manager;
    return true;
}

void image_manager_shutdown(image_manager* manager) {
    for (u32 i = 0; i < manager->image_count; ++i) {
        image_destroy(manager->state, manager->images + i);
    }
    etfree(manager, sizeof(image_manager), MEMORY_TAG_RESOURCE);
}

image* image_get(image_manager* manager, u32 id) {
    return &manager->images[id];
}