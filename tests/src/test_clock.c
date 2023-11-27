#include "test_clock.h"

void test_clock_start(test_clock* clk) {
    clk->start = clock();
}

void test_clock_update(test_clock* clk) {
    clk->end = clock();
}