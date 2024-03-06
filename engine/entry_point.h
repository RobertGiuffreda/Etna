#pragma once

#include "core/engine.h"
#include "core/logger.h"
#include "core/asserts.h"
#include "application_types.h"

extern b8 define_configuration(engine_config* engine_details, application_config* app_details);

int main(int argc, char** argv) {
    ETASSERT(argc > 1);
    ETASSERT(argv[0] != NULL);
    ETASSERT(argv[1] != NULL);

    // Default values
    engine_config engine_details = {
        .width = 100,
        .height = 100,
        .x_start_pos = 0,
        .y_start_pos = 0,
        .scene_path = argv[1],
    };
    application_config app_details = {0};

    if (!define_configuration(&engine_details, &app_details)) {
        // NOTE: This is before logger is setup so no output into log file
        ETFATAL("define_configuration failed to run.");
        return 1;
    }

    if (!engine_initialize(engine_details, app_details)) {
        ETFATAL("Engine Initialize failed.");
        return 1;
    }

    if (!engine_run()) {
        ETDEBUG("Error when running engine.");
        return 1;
    }

    engine_shutdown();
    return 0;
}