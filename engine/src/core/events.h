#pragma once

#include "defines.h"

typedef struct event_data {
    union {
        i64 i64[2];
        u64 u64[2];
        f64 f64[2];

        i32 i32[4];
        u32 u32[4];
        f32 f32[4];

        i16 i16[8];
        u16 u16[8];

        i8 i8[16];
        u8 u8[16];
    };
} event_data;

// Observer callback function
typedef b8 (*pfn_on_event)(u16 event_code, event_data data);

typedef struct events_state_t events_state;

b8 events_initialize(events_state** event_system_state);

void events_shutdown(events_state* event_system_state);

b8 event_observer_register(u16 event_code, void* observer, pfn_on_event on_event);

b8 event_observer_deregister(u16 event_code, void* observer, pfn_on_event on_event);

b8 event_fire(u16 event_code, event_data data);

// System event codes. More for user
typedef enum system_event_code {
    EVENT_CODE_ENGINE_SHUTDOWN = 0x01,

    /* event_data data.u16[0] = key_code */
    EVENT_CODE_KEY_PRESSED = 0x02,

    /* event_data data.u16[0] =  */
    EVENT_CODE_KEY_REPEAT = 0x03,

    /* event_data data.u16[0] = key_code */
    EVENT_CODE_KEY_RELEASED = 0x04,

    /* event_data data.u16[0] = button_code */
    EVENT_CODE_BUTTON_PRESSED = 0x05,

    /* event_data data.u16[0] = button_code */
    EVENT_CODE_BUTTON_RELEASED = 0x06,

    /* event_data data.u32[0] = x
     * event_data data.u32[1] = y
     */
    EVENT_CODE_MOUSE_MOVED = 0x07,

    /* event_data data.u8[0] = z_delta */
    EVENT_CODE_MOUSE_WHEEL = 0x08,

    /* event_data data.u32[0] = width
     * event_data data.u32[1] = height
     */
    EVENT_CODE_RESIZED = 0x09,

    EVENT_CODE_MAX = 0xFF
} system_event_code;