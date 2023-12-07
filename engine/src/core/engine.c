#include "engine.h"

#include "core/asserts.h"
#include "core/etmemory.h"
#include "core/logger.h"
#include "core/events.h"
#include "core/input.h"
#include "core/clock.h"

#include "platform/platform.h"
#include "platform/etwindow.h"

#include "renderer/renderer.h"

#include "application_types.h"

// TODO: Add application name to config

typedef struct engine_state {
    // TODO: Transfer these to a visible window_state_struct or something
    // i32 x_pos;
    // i32 y_pos;
    // i32 width;
    // i32 height;

    b8 is_running;

    // Application data
    b8 (*app_initialize)(application_state* app_state);
    void (*app_shutdown)(application_state* app_state);
    b8 (*app_update)(application_state* app_state);
    b8 (*app_render)(application_state* app_state);

    u64 app_state_size;
    application_state* app_state;

    // TODO: u64 etwindow_state_size for when engine will allocate the memory
    etwindow_state* window_state;

    // TODO: u64 events_state_size for when engine will allocate the memory
    events_state* events_state;

    // TODO: u64 input_state_size for when engine will allocate the memory
    input_state* input_state;

    // TODO: u64 input_state_size for when engine will allocate the memory
    renderer_state* renderer_state;
} engine_state;

static engine_state* state;

b8 engine_on_resize(u16 event_code, event_data data);

b8 engine_initialize(engine_config engine_details, application_config app_details) {
    // Initialize memory first, so all memory allocations are tracked.
    if (!memory_initialize()) {
        ETFATAL("Unable to initialize memory.");
        return false;
    };

    // Allocate engine state memory
    state = (engine_state*)etallocate(sizeof(engine_state), MEMORY_TAG_ENGINE);
    state->is_running = false;

    // Initialize the logger. When log file is implemented this is where it will initialized
    if (!logger_initialize()) {
        ETFATAL("Unable to initialize logger.");
        return false;
    }

    // Check if the application passed the functions
    b8 has_init, has_shutdown, has_update, has_render = false;
    if (!(has_init = app_details.initialize) ||
        !(has_shutdown = app_details.shutdown) ||
        !(has_update = app_details.shutdown) ||
        !(has_render = app_details.render))
    {
        // A necessary function is missing. Exit
        ETFATAL(
            "Initialize: %u | Shutdown %u | Update: %u | Render: %u.",
            has_init, has_shutdown, has_update, has_render
        );
    }

    // Events
    events_initialize(&state->events_state);

    // Input
    input_initialize(&state->input_state);

    // Initialize platform
    if (!platform_initialize()) {
        ETFATAL("Platfrom failed to initialize.");
        return false;
    }

    // Create window
    etwindow_config window_config = {
        .name = "Etna Window",
        .x_start_pos = 100,
        .y_start_pos = 100,
        .width = 720,
        .height = 480
    };
    if (!etwindow_initialize(&window_config, &state->window_state)) {
        ETFATAL("Window failed to initialize.");
        return false;
    }

    // Initialize renderer
    if (!renderer_initialize(&state->renderer_state, state->window_state, "App")) {
        ETFATAL("Renderer failed to initialize.");
        return false;
    }

    // Transfer app information
    state->app_initialize = app_details.initialize;
    state->app_shutdown = app_details.shutdown;
    state->app_update = app_details.update;
    state->app_render = app_details.render;
    state->app_state_size = app_details.state_size;
    state->app_state = (application_state*)etallocate(app_details.state_size, MEMORY_TAG_APPLICATION);

    // Initialize application
    if (!state->app_initialize(state->app_state)) {
        ETFATAL("Unable to initialize application. application_initialize returned false.");
        return false;
    }
    return true;
}

// TODO: Handle minimizing
b8 engine_run(void) {
    state->is_running = true;

    state->app_update(state->app_state);

    while (state->is_running) {
        renderer_draw_frame(state->renderer_state);

        // Belong to platform or window??
        etwindow_pump_messages();
        state->is_running = !etwindow_should_close(state->window_state);
    }
    state->is_running = false;
    return true;
}

void engine_shutdown(void) {
    // Call the passed in shutdown function for the application
    state->app_shutdown(state->app_state);

    // Free the app_state memory
    etfree(state->app_state, state->app_state_size, MEMORY_TAG_APPLICATION);

    renderer_shutdown(state->renderer_state);

    // Shutdown the window
    etwindow_shutdown(state->window_state);

    // Shutdown platform
    platform_shutdown();

    // Shutdown input system
    input_shutdown(state->input_state);

    // Deregister engine events & Shutdown event system
    events_shutdown(state->events_state);

    // Shutdown log file
    logger_shutdown();
    
    // Free memory used for the state
    etfree(state, sizeof(engine_state), MEMORY_TAG_ENGINE);

    // Close memory
    memory_shutdown();
}

b8 engine_on_resize(u16 event_code, event_data data) {
    ETDEBUG("X-pos: %d | Y-pos: %d", data.i32[0], data.i32[1]);

    // Other events should handle this event code as well
    return false;
}