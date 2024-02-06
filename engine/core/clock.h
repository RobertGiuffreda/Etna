#pragma once

#include "defines.h"

// TODO: Remove elapsed and have clock_time just return that value or just have a time memeber variable
typedef struct clock {
    f64 start;
    f64 elapsed;
} clock;

void clock_start(clock* clock);

void clock_time(clock* clock);