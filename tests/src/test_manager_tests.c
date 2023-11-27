#include "test_manager_tests.h"

#include "test_manager.h"

void test_manager_tests_register_all_tests(void) {
    test_manager_register_test(test_manager_tests_test, "This is a test function.");
}

b8 test_manager_tests_test(void) {
    ETINFO("Test func called.");
    return true;
}