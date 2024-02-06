#include "clock.h"

#include "platform/platform.h"

void clock_start(clock* clock) {
    clock->start = platform_get_time();
}

void clock_time(clock* clock) {
    clock->elapsed = platform_get_time() - clock->start;
}