#include "image_manager.h"

#include "memory/etmemory.h"
#include "core/asserts.h"
#include "core/logger.h"
#include "core/etstring.h"

#include "resource_private.h"
#include "renderer/src/renderer.h"
#include "renderer/src/image.h"

b8 image_manager_initialize(image_manager** manager, renderer_state* state) {
    image_manager* new_manager = etallocate(sizeof(image_manager), MEMORY_TAG_RESOURCE);
    etzero_memory(new_manager, sizeof(image_manager));
    new_manager->state = state;

    for (u32 i = 0; i < MAX_IMAGE_COUNT; ++i) {
        new_manager->images[i] = state->default_error;
        new_manager->images[i].id = INVALID_ID;
    }

    *manager = new_manager;
    return true;
}

void image_manager_shutdown(image_manager* manager) {
    for (u32 i = 0; i < manager->image_count; ++i) {
        if (manager->images[i].id == INVALID_ID) {
            continue;
        }
        str_duplicate_free(manager->images[i].name);
        image_destroy(manager->state, manager->images + i);
    }
    etfree(manager, sizeof(image_manager), MEMORY_TAG_RESOURCE);
}

image* image_manager_get(image_manager* manager, u32 id) {
    return &manager->images[id];
}

u32 image2D_submit(image_manager* manager, image2D_config* config) {
    if (manager->image_count >= MAX_IMAGE_COUNT)
        return INVALID_ID;

    u32 id = manager->image_count++;
    image* new_image = &manager->images[id];

    if (config->data) {
        new_image->name = (config->name) ? str_duplicate_allocate(config->name) : str_duplicate_allocate("Unknown Name");
        new_image->id = id;

        VkExtent3D image_size = {
            .width = config->width,
            .height = config->height,
            .depth = 1};
        image2D_create_data(
            manager->state,
            config->data,
            image_size,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            new_image);
        SET_DEBUG_NAME(manager->state, VK_OBJECT_TYPE_IMAGE, new_image->handle, new_image->name);
        SET_DEBUG_NAME(manager->state, VK_OBJECT_TYPE_IMAGE_VIEW, new_image->view, new_image->name);
    }
    return id;
}
