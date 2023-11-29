#include "platform.h"

#include "core/logger.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <vulkan/vulkan.h>

static void platform_error_callback(int error, const char* description);

b8 platform_initialize(void) {
    if (!glfwInit()) {
        ETFATAL("Function glfwInit returned false.");
        return false;
    }
    if (!glfwVulkanSupported()) {
        ETFATAL("Function glfwVulkanSupported returned false.");
        glfwTerminate();
        return false;
    }
    glfwSetErrorCallback(platform_error_callback);
    return true;
}

void platform_shutdown(void) {
    glfwTerminate();
}

f64 platform_get_time(void) {
    return glfwGetTime();
}

void platform_error_callback(int error, const char* description) {
    ETERROR("Error: %s", description);
}