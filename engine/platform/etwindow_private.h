#pragma once
#include "etwindow.h"

#include "renderer/renderer_types.h"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

struct etwindow_t {
    GLFWwindow* impl_window;
    swapchain* swapchain;
    b8 cursor_captured;
};
