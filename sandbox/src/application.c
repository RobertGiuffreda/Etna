#include "application.h"

#include <core/logger.h>

b8 application_initialize(struct application_state_t* state) {
    ETINFO("application_initialize called.");
    return true;
}

void application_shutdown(struct application_state_t* state) {
    ETINFO("application_shutdown called.");
}

b8 application_update(struct application_state_t* state) {
    ETINFO("application_update called.");
    return true;
}

b8 application_render(struct application_state_t* state) {
    ETINFO("application_render called.");
    return true;
}