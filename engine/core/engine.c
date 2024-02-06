#include "engine.h"

#include "core/asserts.h"
#include "memory/etmemory.h"
#include "core/logger.h"
#include "core/events.h"
#include "core/input.h"
#include "core/clock.h"

#include "platform/platform.h"
#include "platform/etwindow.h"

#include "renderer/rendererAPI.h"

#include "scene/scene.h"

#include "application_types.h"

// TODO: Add application name to config

typedef struct engine_state {
    b8 is_running;
    b8 minimized;
    clock frame_clk;

    // HACK:TEMP: Proper scene & project management
    scene* main_scene;
    // HACK:TEMP: END

    // Application data
    b8 (*app_initialize)(application_t* app);
    void (*app_shutdown)(application_t* app);
    b8 (*app_update)(application_t* app);
    b8 (*app_render)(application_t* app);

    u64 app_state_size;
    application_t* app;
    etwindow_state* window_state;
    events_state* events_state;
    input_state* input_state;
    renderer_state* renderer_state;
} engine_state;

static engine_state* state;

static b8 engine_on_resize(u16 event_code, void* engine_state, event_data data);
static b8 engine_on_key_event(u16 event_code, void* engine_state, event_data data);

b8 engine_initialize(engine_config engine_details, application_config app_details) {
    // Initialize memory first, so all memory allocations are tracked.
    if (!memory_initialize()) {
        ETFATAL("Unable to initialize memory.");
        return false;
    }

    // Allocate engine state memory
    state = (engine_state*)etallocate(sizeof(engine_state), MEMORY_TAG_ENGINE);
    state->is_running = false;

    // Initialize the logger. When log file is implemented this is where it will initialized
    if (!logger_initialize()) {
        ETFATAL("Unable to initialize logger.");
        return false;
    }
    ETFATAL("Testing fatal.");
    ETERROR("Testing error.");
    ETWARN("Testing warn.");
    ETINFO("Testing info.");
    ETDEBUG("Testing debug");
    ETTRACE("Testing trace");


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
        .x_start_pos = engine_details.x_start_pos,
        .y_start_pos = engine_details.y_start_pos,
        .width = engine_details.width,
        .height = engine_details.height
    };
    if (!etwindow_initialize(&window_config, &state->window_state)) {
        ETFATAL("Window failed to initialize.");
        return false;
    }

    state->minimized = false;

    // Initialize renderer
    if (!renderer_initialize(&state->renderer_state, state->window_state, "App")) {
        ETFATAL("Renderer failed to initialize.");
        return false;
    }

    // HACK:TEMP: Loading scene should happen in editor/application
    scene_initalize(&state->main_scene, state->renderer_state);
    // HACK:TEMP: END

    event_observer_register(EVENT_CODE_KEY_RELEASE, (void*)state, engine_on_key_event);
    event_observer_register(EVENT_CODE_RESIZE, (void*)state, engine_on_resize);

    // Transfer app information
    state->app_initialize = app_details.initialize;
    state->app_shutdown = app_details.shutdown;
    state->app_update = app_details.update;
    state->app_render = app_details.render;
    state->app_state_size = app_details.app_size;
    state->app = (application_t*)etallocate(app_details.app_size, MEMORY_TAG_APPLICATION);

    // Initialize application
    if (!state->app_initialize(state->app)) {
        ETFATAL("Unable to initialize application. application_initialize returned false.");
        return false;
    }
    return true;
}

b8 engine_run(void) {
    state->is_running = true;

    // TEMP: Better clocking needs to be implemented
    while (state->is_running && !etwindow_should_close(state->window_state)) {
        clock_start(&state->frame_clk);
        state->app_update(state->app);

        if (!state->minimized) {
            // HACK: scene update changes renderer state.
            scene_update(state->main_scene);
            renderer_draw_frame(state->renderer_state);
        }
        input_update(state->input_state);
        etwindow_poll_events(); // glfwPollEvents() called
        clock_time(&state->frame_clk);
    }
    
    return true;
}

void engine_shutdown(void) {
    // Call the passed in shutdown function for the application
    state->app_shutdown(state->app);

    // Free the app memory
    etfree(state->app, state->app_state_size, MEMORY_TAG_APPLICATION);

    scene_shutdown(state->main_scene);

    renderer_shutdown(state->renderer_state);

    // Shutdown the window
    etwindow_shutdown(state->window_state);

    // Shutdown platform
    platform_shutdown();

    // Shutdown input system
    input_shutdown(state->input_state);

    // Deregister engine events & Shutdown event system
    event_observer_deregister(EVENT_CODE_RESIZE, (void*)state, engine_on_resize);
    event_observer_deregister(EVENT_CODE_KEY_RELEASE, (void*)state, engine_on_key_event);
    events_shutdown(state->events_state);

    // Shutdown log file
    log_memory_metrics();
    
    logger_shutdown();
    
    // Free memory used for the state
    etfree(state, sizeof(engine_state), MEMORY_TAG_ENGINE);

    // Close memory
    memory_shutdown();
}

b8 engine_on_resize(u16 event_code, void* engine_state, event_data data) {
    if (EVENT_DATA_WIDTH(data) == 0 || EVENT_DATA_HEIGHT(data) == 0) {
        state->minimized = true;
    } else {
        state->minimized = false;
    }

    // TODO: Register renderer for resizes using events
    renderer_on_resize(state->renderer_state, EVENT_DATA_WIDTH(data), EVENT_DATA_HEIGHT(data));
    // Other events should handle this event code as well, so false
    return false;
}

b8 engine_on_key_event(u16 event_code, void* engine_state, event_data data) {
    keys key = EVENT_DATA_KEY(data);
    if (event_code == EVENT_CODE_KEY_RELEASE) {
        switch (key)
        {
        case KEY_ESCAPE:
            state->is_running = false;
            break;
        case KEY_F:
            // HACK: I'm sorry, this is awful. But I don't want to implement a GUI before I am ready
            ETINFO("Last frame time: %llf ms.", state->frame_clk.elapsed * 1000);
            break;
        default:
            break;
        }
    }
    return true;
}