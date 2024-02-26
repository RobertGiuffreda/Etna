#include "renderer_window.h"
#include "memory/etmemory.h"
#include "platform/etwindow_private.h"
#include "renderer/src/swapchain.h"
#include "renderer/src/renderer.h"

b8 renderer_window_init(renderer_state* renderer_state, etwindow_t* window) {
    window->swapchain = etallocate(sizeof(swapchain), MEMORY_TAG_WINDOW);
    return false;
}

b8 window_create_vulkan_surface(renderer_state* renderer_state, etwindow_t* window) {
    VkResult result = glfwCreateWindowSurface(
        renderer_state->instance,
        window->impl_window,
        renderer_state->allocator,
        &renderer_state->swapchain.surface);
    return result == VK_SUCCESS;
}

const char** window_get_required_extension_names(i32* count) {
    return glfwGetRequiredInstanceExtensions(count);
}