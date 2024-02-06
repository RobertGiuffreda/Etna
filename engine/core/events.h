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

        char* c[16];
    };
} event_data;

#define EVENT_DATA_MOUSE_X(edata) edata.i32[0]
#define EVENT_DATA_MOUSE_Y(edata) edata.i32[1]
#define EVENT_DATA_MOUSE_WHEEL(edata) edata.u8[0]
#define EVENT_DATA_WIDTH(edata) edata.i32[0]
#define EVENT_DATA_HEIGHT(edata) edata.i32[1]

// Observer callback function
typedef b8 (*pfn_on_event)(u16 event_code, void* observer, event_data data);

typedef struct events_state_t events_state;

b8 events_initialize(events_state** event_system_state);

void events_shutdown(events_state* event_system_state);

b8 event_observer_register(u16 event_code, void* observer, pfn_on_event on_event);

b8 event_observer_deregister(u16 event_code, void* observer, pfn_on_event on_event);

b8 event_fire(u16 event_code, event_data data);

// TODO: EVENT_CODE --> ETNA or EVENT
typedef enum system_event {
    EVENT_CODE_ENGINE_SHUTDOWN = 0x01,
    EVENT_CODE_KEY_PRESS = 0x02,
    EVENT_CODE_KEY_REPEAT = 0x03,
    EVENT_CODE_KEY_RELEASE = 0x04,
    EVENT_CODE_BUTTON_PRESS = 0x05,
    EVENT_CODE_BUTTON_RELEASE = 0x06,
    EVENT_CODE_MOUSE_MOVE = 0x07,
    EVENT_CODE_MOUSE_WHEEL = 0x08,
    EVENT_CODE_RESIZE = 0x09,
    EVENT_CODE_MAX = 0xFF,
} system_event;