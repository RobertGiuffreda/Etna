#pragma once

#include "core/engine.h"
#include "core/logger.h"
#include "application_types.h"

extern b8 define_configuration(engine_config* engine_details, application_config* app_details);

int main(int argc, char** argv) {
    engine_config engine_details;
    application_config app_details;

    if (!define_configuration(&engine_details, &app_details)) {
        // NOTE: This is before logger is setup so no output
        ETFATAL("define_configuration failed to run.");
        return -1;
    }

    engine_initialize(engine_details, app_details);
    engine_run();
    engine_shutdown();
}