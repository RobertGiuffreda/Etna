#pragma once
#include "defines.h"

typedef struct etwindow_config {
    const char* name;
    i32 x_start_pos;
    i32 y_start_pos;
    u32 width;
    u32 height;
} etwindow_config;

typedef struct etwindow_t etwindow_t;

b8 etwindow_initialize(etwindow_config* config, etwindow_t** out_window_state);

void etwindow_shutdown(etwindow_t* window);

b8 etwindow_should_close(etwindow_t* window);

void etwindow_poll_events(void);