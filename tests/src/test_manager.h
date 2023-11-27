#pragma once
#include <defines.h>

#define BYPASS 2

typedef u8 (*func_test)();

void test_manager_initialize(void);

void test_manager_shutdown(void);

void test_manager_register_test(func_test test, const char* desc);

void test_manager_run_tests(void);