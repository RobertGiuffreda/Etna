#pragma once
#include <defines.h>

#include <time.h>

typedef struct test_clock {
    clock_t start;
    clock_t end;
} test_clock;

#define TEST_CLOCK_TO_SEC(start, end) (((end - start)/(f64)CLOCKS_PER_SEC))

void test_clock_start(test_clock* clk);

void test_clock_update(test_clock* clk);