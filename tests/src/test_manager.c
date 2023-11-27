#include "test_manager.h"
#include "test_clock.h"

#include <core/etmemory.h>
#include <core/logger.h>
#include <containers/dynarray.h>

typedef struct test_entry {
    func_test test;
    const char* desc;
} test_entry;

typedef struct test_manager_s {
    test_entry* tests;
} test_manager_t;

static test_entry* tests;

void test_manager_initialize(void) {
    tests = (test_entry*)dynarray_create(0, sizeof(test_entry));
}

void test_manager_shutdown(void) {
    dynarray_destroy(tests);
}

void test_manager_register_test(func_test test, const char* desc) {
    test_entry temp_entry; 
    temp_entry.test = test;
    temp_entry.desc = desc;
    dynarray_push((void**)&tests, &temp_entry);
}

void test_manager_run_tests(void) {
    u32 passed = 0;
    u32 failed = 0;
    u32 skipped = 0;

    u32 count = dynarray_length(tests);

    test_clock total_timer;
    test_clock_start(&total_timer);
    for (i32 i = 0; i < count; ++i) {
        test_clock test_timer;
        test_clock_start(&test_timer);
        b8 result = tests[i].test();
        test_clock_update(&test_timer);
        test_clock_update(&total_timer);

        if (result == true) {
            ETINFO("Test %d PASSED.", i);
            passed++;
        } else if (result == BYPASS) {
            ETWARN("Test %d SKIPPED. '%s", i, tests[i].desc);
            skipped++;
        } else {
            ETERROR("Test %d FAILED. '%s", i, tests[i].desc);
            failed++;
        }

        f64 test_time = TEST_CLOCK_TO_SEC(test_timer.start, test_timer.end);
        f64 total_time = TEST_CLOCK_TO_SEC(total_timer.start, total_timer.end);

        ETINFO("Executed %d of %d. %d skipped. ***Failed: %d*** | ***Passed: %d***. (%.6lfs of %.6lfs).", (i + 1), count, skipped, failed, passed, test_time, total_time);
    }
    ETINFO("Results: %d passed, %d failed, %d skipped.", passed, failed, skipped);
}