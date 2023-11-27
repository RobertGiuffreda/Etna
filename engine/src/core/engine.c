#include "engine.h"

#include "core/etmemory.h"
#include "core/logger.h"
#include "core/asserts.h"

#include "application_types.h"

typedef struct engine_state {
    i32 x_pos;
    i32 y_pos;
    i32 width;
    i32 height;

    // Application functions
    b8 (*app_initialize)(application_state* app_state);
    void (*app_shutdown)(application_state* app_state);
    b8 (*app_update)(application_state* app_state);
    b8 (*app_render)(application_state* app_state);

    u64 app_state_size;
    application_state* app_state;
} engine_state;

static engine_state* state;

b8 engine_initialize(engine_config engine_details, application_config app_details) {
    // Initialize memory first, so all memory allocations are tracked.
    if (!memory_initialize()) {
        ETFATAL("Unable to initialize memory.");
        return false;
    };

    // Initialize engine state
    state = (engine_state*)etallocate(sizeof(engine_state), MEMORY_TAG_ENGINE);

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

b8 engine_run(void) {
    if (!state->app_update(state->app_state)) {
        ETFATAL("application_update returned false.");
        return false;
    }
    return true;
}

void engine_shutdown(void) {

    // Call the passed in shutdown function for the application
    state->app_shutdown(state->app_state);

    // Free the app_state memory
    etfree(state->app_state, state->app_state_size, MEMORY_TAG_APPLICATION);

    // Shutdown log file
    logger_shutdown();
    
    // Free memory used for the state
    etfree(state, sizeof(engine_state), MEMORY_TAG_ENGINE);

    // Close memory
    memory_shutdown();
}