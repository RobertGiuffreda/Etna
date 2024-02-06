#include "platform.h"

#include "core/logger.h"

#if defined(ET_WINDOWS)
#include <Windows.h>
#endif

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>


static void glfw_error_callback(int error, const char* description);

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
    glfwSetErrorCallback(glfw_error_callback);
    return true;
}

void platform_shutdown(void) {
    glfwTerminate();
}

f64 platform_get_time(void) {
    return glfwGetTime();
}

b8 platform_init_ANSI_escape(void) {
#if defined(ET_WINDOWS)
    // TODO: GetLastError 
    DWORD hout_mode = 0;
    HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hout == INVALID_HANDLE_VALUE || !GetConsoleMode(hout, &hout_mode)) {
        return false;
    }

    DWORD hin_mode = 0;
    HANDLE hin = GetStdHandle(STD_INPUT_HANDLE);
    if (hin == INVALID_HANDLE_VALUE || !GetConsoleMode(hin, &hin_mode)) {
        return false;
    }

    DWORD out_mode = hout_mode |= (ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    DWORD in_mode = hin_mode |= ENABLE_VIRTUAL_TERMINAL_INPUT;
    if (!SetConsoleMode(hout, out_mode) || !SetConsoleMode(hin, in_mode)) {
        return false;
    }
#endif
    return true;
}

void glfw_error_callback(int error, const char* description) {
    ETERROR("Error: %s", description);
}