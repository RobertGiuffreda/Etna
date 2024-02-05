#include "application.h"

#include <application_types.h>
#include <memory/etmemory.h>
#include <core/logger.h>
#include <math/math_types.h>
#include <core/etfile.h>

struct application_state_t {
    u32 temp;
};

b8 application_initialize(struct application_state_t* state) {
    // Zero memory allocated for the application state 
    etzero_memory(state, sizeof(struct application_state_t));
    return true;
}

void application_shutdown(struct application_state_t* state) {}

b8 application_update(struct application_state_t* state) {
    return true;
}

b8 application_render(struct application_state_t* state) {
    return true;
}

u64 get_appstate_size(void) {
    return sizeof(struct application_state_t);
}