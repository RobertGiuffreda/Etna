#include <defines.h>
#include <core/engine.h>
#include <application_types.h>

#include <entry_point.h>

#include "application.h"

b8 define_configuration(engine_config* engine_details, application_config* app_details) {
    engine_details->x_start_pos = 0;
    engine_details->y_start_pos = 0;

    engine_details->width = 1600;
    engine_details->height = 900;

    app_details->initialize = application_initialize;
    app_details->shutdown = application_shutdown;
    app_details->update = application_update;
    app_details->render = application_render;

    app_details->app_size = get_appstate_size();
    return true;
}