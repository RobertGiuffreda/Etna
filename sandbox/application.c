#include "application.h"

#include <application_types.h>
#include <core/logger.h>
#include <core/etfile.h>
#include <math/math_types.h>
#include <memory/etmemory.h>
#include <scene/scene.h>

struct application_t {
    scene* main_scene;
};

b8 application_initialize(application_t* app) {
    // Zero memory allocated for the app
    etzero_memory(app, sizeof(application_t));
    return true;
}

void application_shutdown(application_t* app) {
    // scene_shutdown(app->main_scene);
}

b8 application_update(application_t* app) {
    // scene_update(app->main_scene);
    return true;
}

b8 application_render(application_t* app) {
    return true;
}

u64 get_appstate_size(void) {
    return sizeof(application_t);
}