#pragma once

#include "defines.h"

typedef struct clock {
    f64 start;
    f64 elapsed;
} clock;

void clock_start(clock* clock);

void clock_time(clock* clock);