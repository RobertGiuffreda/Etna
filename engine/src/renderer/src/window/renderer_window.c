#include "renderer_window.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

struct etwindow_state {
    GLFWwindow* impl_window;
    b8 cursor_captured;
};

b8 window_create_vulkan_surface(renderer_state* renderer_state, etwindow_state* window_state) {
    VkResult result = glfwCreateWindowSurface(
        renderer_state->instance,
        window_state->impl_window,
        renderer_state->allocator,
        &renderer_state->surface);
    return result == VK_SUCCESS;
}

const char** window_get_required_extension_names(i32* count) {
    return glfwGetRequiredInstanceExtensions(count);
}