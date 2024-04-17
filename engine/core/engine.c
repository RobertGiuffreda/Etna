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
#include "resources/importers/importer.h"

#include "application_types.h"

// TODO: Add application name to config
typedef struct engine_t {
    b8 is_running;
    b8 is_minimized;
    clock frame;

    // HACK:TEMP: Proper scene management
    scene* main_scene;
    // HACK:TEMP: END

    u64 app_size;
    application_t* app;
    b8 (*app_initialize)(application_t* app);
    void (*app_shutdown)(application_t* app);
    b8 (*app_update)(application_t* app, f64 dt);
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

    engine = etallocate(sizeof(engine_t), MEMORY_TAG_ENGINE);
    engine->is_running = false;
    engine->is_minimized = false;
    engine->frame.start = 0;
    engine->frame.elapsed = 0;

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

    events_initialize(&engine->event_system);

    input_initialize(&engine->input_state);

    if (!platform_initialize()) {
        ETFATAL("Platfrom failed to initialize.");
        return false;
    }

    etwindow_config window_config = {
        .name = "Etna Window",
        .x_start_pos = engine_details.x_start_pos,
        .y_start_pos = engine_details.y_start_pos,
        .width = engine_details.width,
        .height = engine_details.height};
    if (!etwindow_initialize(&window_config, &engine->window)) {
        ETFATAL("Window failed to initialize.");
        return false;
    }

    renderer_config renderer_config = {
        .app_name = "Test Application",
        .engine_name = "Etna",
        .window = engine->window,
        .frame_overlap = 3};
    if (!renderer_initialize(&engine->renderer_state, renderer_config)) {
        ETFATAL("Renderer failed to initialize.");
        return false;
    }

    import_payload test_payload = import_files(
        engine_details.path_count,
        engine_details.paths);
    scene_config scene_config = {
        .name = "Etna Scene Testing",
        .resolution_width = engine_details.width,
        .resolution_height = engine_details.height,
        .renderer_state = engine->renderer_state,
        .import_payload = &test_payload};
    if (!scene_init(&engine->main_scene, scene_config)) {
        ETFATAL("Unable to initialize scene from payload.");
        return false;
    }

    event_observer_register(EVENT_CODE_KEY_RELEASE, (void*)engine, engine_on_key_event);
    event_observer_register(EVENT_CODE_RESIZE, (void*)engine, engine_on_resize);

    b8 has_init = false, has_shutdown = false, has_update = false, has_render = false;
    if (!(has_init = app_details.initialize) ||
        !(has_shutdown = app_details.shutdown) ||
        !(has_update = app_details.update) ||
        !(has_render = app_details.render)
    ) {
        ETFATAL("Initialize: %u | Shutdown %u | Update: %u | Render: %u.",
            has_init, has_shutdown, has_update, has_render);
        return false;
    }

    engine->app_initialize = app_details.initialize;
    engine->app_shutdown = app_details.shutdown;
    engine->app_update = app_details.update;
    engine->app_render = app_details.render;
    engine->app_size = app_details.app_size;
    engine->app = etallocate(app_details.app_size, MEMORY_TAG_APPLICATION);

    if (!engine->app_initialize(engine->app)) {
        ETFATAL("Unable to initialize application. application_initialize returned false.");
        return false;
    }

    return true;
}

b8 engine_run(void) {
    engine->is_running = true;
    clock_start(&engine->frame);

    while (engine->is_running && !etwindow_should_close(engine->window)) {
        if (!engine->is_minimized) {
            clock_time(&engine->frame);
            f64 dt = engine->frame.elapsed;
            printf("Frame time: %.8llfms\r", dt * 1000);
            clock_start(&engine->frame);

            scene_update(engine->main_scene, dt);
            engine->app_update(engine->app, dt);
            
            if (scene_frame_begin(engine->main_scene, engine->renderer_state)) {
                scene_render(engine->main_scene, engine->renderer_state);
                engine->app_render(engine->app);
                scene_frame_end(engine->main_scene, engine->renderer_state);
            }
        }

        input_update(engine->input_state);
        etwindow_poll_events(); // glfwPollEvents() called
    }
    
    return true;
}

void engine_shutdown(void) {
    engine->app_shutdown(engine->app);

    etfree(engine->app, engine->app_size, MEMORY_TAG_APPLICATION);

    scene_shutdown(engine->main_scene);

    renderer_shutdown(engine->renderer_state);

    etwindow_shutdown(engine->window);

    platform_shutdown();

    input_shutdown(engine->input_state);

    event_observer_deregister(EVENT_CODE_RESIZE, (void*)engine, engine_on_resize);
    event_observer_deregister(EVENT_CODE_KEY_RELEASE, (void*)engine, engine_on_key_event);
    events_shutdown(engine->event_system);
    
    logger_shutdown();
    
    etfree(engine, sizeof(engine_t), MEMORY_TAG_ENGINE);

    print_memory_metrics();
    memory_shutdown();
}

b8 engine_on_resize(u16 event_code, void* engine_state, event_data data) {
    engine->is_minimized = EVENT_DATA_WIDTH(data) == 0 || EVENT_DATA_HEIGHT(data) == 0;
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
        default:
            break;
        }
    }
    return true;
}