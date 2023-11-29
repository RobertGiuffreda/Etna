#include "etwindow.h"

#include "core/etmemory.h"
#include "core/logger.h"
#include "core/events.h"
#include "core/input.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

struct etwindow_state {
    GLFWwindow* impl_window;
};

static void key_callback(GLFWwindow* window, i32 key, i32 scancode, i32 action, i32 mods);
static void mouse_button_callback(GLFWwindow* window, i32 button, i32 action, i32 mods);
static void cursor_position_callback(GLFWwindow* window, f64 xpos, f64 ypos);
static void scroll_callback(GLFWwindow* window, f64 xoffset, f64 yoffset);
static void resize_callback(GLFWwindow* window, i32 width, i32 height);

// TODO: Window user pointer concept from glfw sounds great
b8 etwindow_initialize(etwindow_config* config, etwindow_state** out_window_state) {
    // Setup window using GLFW_NO_API as per glfw vulkan guide
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(config->width, config->height, config->name, 0, 0);
    if (!window) {
        ETFATAL("Failed to create glfw window. Function glfwCreateWindow returned false.");
        return false;
    }

    // Register callback functions
    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetWindowSizeCallback(window, resize_callback);


    glfwSetWindowPos(window, config->x_start_pos, config->y_start_pos);
    glfwShowWindow(window);

    *out_window_state = (etwindow_state*)etallocate(sizeof(struct etwindow_state), MEMORY_TAG_WINDOW);
    (*out_window_state)->impl_window = window;
    return true;
}

void etwindow_shutdown(etwindow_state* window_state) {
    // Destroy window & free state memory
    glfwDestroyWindow(window_state->impl_window);
    etfree(window_state, sizeof(struct etwindow_state), MEMORY_TAG_WINDOW);
}

b8 etwindow_should_close(etwindow_state* window_state) {
    return glfwWindowShouldClose(window_state->impl_window);
}

void etwindow_pump_messages(void) {
    glfwPollEvents();
}

// Action possible values are GLFW_PRESS = 1, GLFW_RELEASE = 0, GLFW_REPEAT = 2. Fits in u8
void key_callback(GLFWwindow* window, i32 key, i32 scancode, i32 action, i32 mods) {
    input_process_key((keys)key, (u8)action);
}

// Action possible values are GLFW_PRESS = 1, GLFW_RELEASE = 0,
void mouse_button_callback(GLFWwindow* window, i32 button, i32 action, i32 mods) {
    input_process_button((buttons)button, (b8)action);
}

void cursor_position_callback(GLFWwindow* window, f64 xpos, f64 ypos) {
    input_process_mouse_move((i32)xpos, (i32)ypos);
}

// TODO: Change scrolling to accept a larger value
void scroll_callback(GLFWwindow* window, f64 xoffset, f64 yoffset) {
    input_process_mouse_wheel((u8)yoffset);
}

void resize_callback(GLFWwindow* window, i32 width, i32 height) {
    event_data e;
    e.i32[0] = width;
    e.i32[1] = height;
    event_fire(EVENT_CODE_RESIZED, e);
}