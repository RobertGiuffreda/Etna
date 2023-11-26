#include "engine.h"

#include "etmemory.h"
#include "logger.h"

b8 engine_initialize() {
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

b8 engine_run() {
    
    return true;
}

void engine_shutdown() {
    logger_shutdown();
    memory_shutdown();
}