#include "events.h"

#include "core/asserts.h"
#include "core/logger.h"
#include "core/etmemory.h"

#include "containers/dynarray.h"

/* Events are immediate & blocking for now. */
/* Queue eventually; To make events deffered. */

typedef struct event_observer {
    void* observer;
    pfn_on_event on_event;
} event_observer;

// typedef struct deferred_event {
//     u16 code;
//     event_data data;
// } deferred_event;

// If event handling order is needed: Either sorting algorithm, or a data struct that inserts in the correct order
#define MAX_MESSAGE_CODES 16384
struct events_state_t {
    // NOTE: Each entry is a dynarray if an observer is registered there
    event_observer* immediate_observers[MAX_MESSAGE_CODES];     // TODO: Data struct to support insertions with ordering(priority) value
    // event_observer* deferred_observers[MAX_MESSAGE_CODES];   // TODO: Data struct to support insertions with ordering(priority) value

    // deferred_event* deferred_events                          // TODO: Data struct to support insertions with ordering(priority) value
};

static struct events_state_t* system_state;

b8 events_initialize(events_state** event_system_state) {
    *event_system_state = (events_state*)etallocate(sizeof(struct events_state_t), MEMORY_TAG_EVENTS);
    etzero_memory(*event_system_state, sizeof(struct events_state_t));
    system_state = *event_system_state;

    return true;
}

void events_shutdown(events_state* event_system_state) {
    for (i32 i = 0; i < MAX_MESSAGE_CODES; ++i) {
        // If immediate observers has been initialized then we destroy the dynarray
        if (event_system_state->immediate_observers[i]) {
            dynarray_destroy(event_system_state->immediate_observers[i]);
        }
    }
    etfree(event_system_state, sizeof(struct events_state_t), MEMORY_TAG_EVENTS);
}

b8 event_observer_register(u16 event_code, void* observer, pfn_on_event on_event) {
    ETASSERT((system_state != 0));

    if (system_state->immediate_observers[event_code] == 0) {
        system_state->immediate_observers[event_code] = dynarray_create(1, sizeof(event_observer));
    }

    event_observer* i_obs = system_state->immediate_observers[event_code];
    u64 i_obs_count = dynarray_length(i_obs);
    for (u64 i = 0; i < i_obs_count; ++i) {
        if (on_event == i_obs[i].on_event && observer == i_obs[i].on_event) {
            ETWARN("This observer has already registered this on_event function.");
            return false;
        }
    }

    event_observer new_event_observer;
    new_event_observer.observer = observer;
    new_event_observer.on_event = on_event;
    dynarray_push((void**)&i_obs, &new_event_observer);

    return true;
}

b8 event_observer_deregister(u16 event_code, void* observer, pfn_on_event on_event) {
    ETASSERT((system_state != 0));

    if (system_state->immediate_observers[event_code] == 0) {
        ETWARN("Attempt to deregister absent observer entry.");
        return false;
    }

    event_observer* i_obs = system_state->immediate_observers[event_code];
    u64 i_obs_count = dynarray_length(i_obs);
    for (u64 i = 0; i < i_obs_count; ++i) {
        event_observer eo = i_obs[i];
        if (on_event == eo.on_event && observer == eo.observer) {
            dynarray_remove(i_obs, &eo, i);
            return true;
        }
    }

    ETWARN("Attempt to deregister absent observer entry.");
    return false;
}

b8 event_fire(u16 event_code, event_data data) {
    ETASSERT((system_state != 0));

    // If nothing is registered for the code, boot out.
    if (system_state->immediate_observers[event_code] == 0) {
        // Some events may be unhandled for now so no warnings or info yet.
        return false;
    }

    event_observer* i_obs = system_state->immediate_observers[event_code];
    u64 i_obs_count = dynarray_length(i_obs);
    for (u64 i = 0; i < i_obs_count; ++i) {
        event_observer eo = i_obs[i];
        if (eo.on_event(event_code, eo.observer, data)) {
            // on_event returning true means it has handled the event completly & 
            // other observers will not see it
            return true;
        }
    }

    // Not found.
    return false;
}