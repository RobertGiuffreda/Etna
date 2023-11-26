#include "engine.h"

#include "etmemory.h"
#include "logger.h"
#include "asserts.h"

b8 engine_initialize(void) {
    if (!memory_initialize()) {
        ETFATAL("Unable to initialize memory.");
        return false;
    };
    if (!logger_initialize()) {
        ETFATAL("Unable to initialize logger.");
        return false;
    }
    return true;
}

b8 engine_run(void) {
    return true;
}

void engine_shutdown(void) {
    logger_shutdown();
    memory_shutdown();
}