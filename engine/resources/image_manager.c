#include "image_manager.h"

#include "memory/etmemory.h"
#include "core/asserts.h"
#include "core/logger.h"
#include "core/etstring.h"

// TEMP: This should be made renderer implementation agnostic
#include "resource_private.h"
#include "renderer/src/image.h"
// TEMP: END

b8 image_manager_initialize(image_manager** manager, renderer_state* state) {
    image_manager* new_manager = etallocate(sizeof(image_manager), MEMORY_TAG_RESOURCE);
    etzero_memory(new_manager, sizeof(image_manager));
    new_manager->state = state;

    for (u32 i = 0; i < MAX_IMAGE_COUNT; ++i) {
        new_manager->images[i] = state->error_image;
        new_manager->images[i].id = INVALID_ID;
    }

    *manager = new_manager;
    return true;
}

void image_manager_shutdown(image_manager* manager) {
    for (u32 i = 0; i < manager->image_count; ++i) {
        // Skip to not destroy the error image
        if (manager->images[i].id == INVALID_ID) {
            continue;
        }
        str_duplicate_free(manager->images[i].name);
        image_destroy(manager->state, manager->images + i);
    }
    etfree(manager, sizeof(image_manager), MEMORY_TAG_RESOURCE);
}

// TEMP: Function to increment the image count to skip over images
// that do not load properly leaving the error_image reference 
void image_manager_increment(image_manager* manager) {
    manager->image_count++;
}
// TEMP: END

image* image_manager_get(image_manager* manager, u32 id) {
    return &manager->images[id];
}

b8 image2D_submit(image_manager* manager, image_config* config) {
    if (manager->image_count >= MAX_IMAGE_COUNT)
        return false;

    image* new_image = &manager->images[manager->image_count];
    new_image->id = manager->image_count;
    if (config->name) {
        new_image->name = str_duplicate_allocate(config->name);
    } else {
        new_image->name = str_duplicate_allocate("Name not found.");
    }
    
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

    manager->image_count++;
    return true;
}
