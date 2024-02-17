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
typedef struct engine_t {
    b8 is_running;
    b8 is_minimized;
    clock frame_clk;

    // HACK:TEMP: Proper scene management
    scene* main_scene;
    // HACK:TEMP: END

    u64 app_size;
    application_t* app;
    b8 (*app_initialize)(application_t* app);
    void (*app_shutdown)(application_t* app);
    b8 (*app_update)(application_t* app);
    b8 (*app_render)(application_t* app);

    etwindow_t* window;
    events_t* event_system;
    input_t* input_state;
    renderer_state* renderer_state;
} engine_t;

static engine_t* engine;

static b8 engine_on_resize(u16 event_code, void* engine_state, event_data data);
static b8 engine_on_key_event(u16 event_code, void* engine_state, event_data data);

b8 engine_initialize(engine_config engine_details, application_config app_details) {
    // Initialize memory first, so all memory allocations are tracked.
    if (!memory_initialize()) {
        ETFATAL("Unable to initialize memory.");
        return false;
    }

    // Allocate engine engine memory
    engine = (engine_t*)etallocate(sizeof(engine_t), MEMORY_TAG_ENGINE);
    engine->is_running = false;

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
        !(has_update = app_details.update) ||
        !(has_render = app_details.render))
    {
        // A necessary function is missing. Exit
        ETFATAL(
            "Initialize: %u | Shutdown %u | Update: %u | Render: %u.",
            has_init, has_shutdown, has_update, has_render
        );
    }

    // Events
    events_initialize(&engine->event_system);

    // Input
    input_initialize(&engine->input_state);

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
    if (!etwindow_initialize(&window_config, &engine->window)) {
        ETFATAL("Window failed to initialize.");
        return false;
    }

    engine->is_minimized = false;

    // Initialize renderer
    if (!renderer_initialize(&engine->renderer_state, engine->window, "App")) {
        ETFATAL("Renderer failed to initialize.");
        return false;
    }

    // HACK:TEMP: Loading scene should happen in editor/application
    scene_initalize(&engine->main_scene, engine->renderer_state);
    // HACK:TEMP: END

    event_observer_register(EVENT_CODE_KEY_RELEASE, (void*)engine, engine_on_key_event);
    event_observer_register(EVENT_CODE_RESIZE, (void*)engine, engine_on_resize);

    // Transfer app information
    engine->app_initialize = app_details.initialize;
    engine->app_shutdown = app_details.shutdown;
    engine->app_update = app_details.update;
    engine->app_render = app_details.render;
    engine->app_size = app_details.app_size;
    engine->app = (application_t*)etallocate(app_details.app_size, MEMORY_TAG_APPLICATION);

    // Initialize application
    if (!engine->app_initialize(engine->app)) {
        ETFATAL("Unable to initialize application. application_initialize returned false.");
        return false;
    }
    return true;
}

b8 engine_run(void) {
    engine->is_running = true;

    while (engine->is_running && !etwindow_should_close(engine->window)) {
        if (!engine->is_minimized) {
            scene_update(engine->main_scene);
            engine->app_update(engine->app);
            
            if (renderer_prepare_frame(engine->renderer_state)) {
                // TEMP: Very rough frame timing mechanism
                clock_time(&engine->frame_clk);
                clock_start(&engine->frame_clk);
                // TEMP: END

                scene_render(engine->main_scene);
                engine->app_render(engine->app);
                renderer_draw_frame(engine->renderer_state);
            }
        }

        input_update(engine->input_state);
        etwindow_poll_events(); // glfwPollEvents() called
    }
    
    return true;
}

void engine_shutdown(void) {
    // Call the passed in shutdown function for the application
    engine->app_shutdown(engine->app);

    // Free the app memory
    etfree(engine->app, engine->app_size, MEMORY_TAG_APPLICATION);

    scene_shutdown(engine->main_scene);

    renderer_shutdown(engine->renderer_state);

    // Shutdown the window
    etwindow_shutdown(engine->window);

    // Shutdown platform
    platform_shutdown();

    // Shutdown input system
    input_shutdown(engine->input_state);

    // Deregister engine events & Shutdown event system
    event_observer_deregister(EVENT_CODE_RESIZE, (void*)engine, engine_on_resize);
    event_observer_deregister(EVENT_CODE_KEY_RELEASE, (void*)engine, engine_on_key_event);
    events_shutdown(engine->event_system);

    
    // Shutdown log file
    logger_shutdown();
    
    // Free memory used for the engine
    etfree(engine, sizeof(engine_t), MEMORY_TAG_ENGINE);

    // Close memory
    print_memory_metrics();
    memory_shutdown();
}

b8 engine_on_resize(u16 event_code, void* engine_state, event_data data) {
    engine->is_minimized = EVENT_DATA_WIDTH(data) == 0 || EVENT_DATA_HEIGHT(data) == 0;

    // TODO: Register renderer for resizes using events
    renderer_on_resize(engine->renderer_state, EVENT_DATA_WIDTH(data), EVENT_DATA_HEIGHT(data));
    // TODO: END

    // Other events should handle this event code as well, so false
    return false;
}

b8 engine_on_key_event(u16 event_code, void* engine_state, event_data data) {
    keys key = EVENT_DATA_KEY(data);
    if (event_code == EVENT_CODE_KEY_RELEASE) {
        switch (key)
        {
        case KEY_ESCAPE:
            engine->is_running = false;
            break;
        case KEY_F:
            // HACK: I'm sorry, this is awful. But I don't want to implement a GUI before I am ready
            ETINFO("Last frame time: %llf ms.", engine->frame_clk.elapsed * 1000);
            break;
        default:
            break;
        }
    }
    return true;
}