#include <defines.h>

#include "test_manager.h"
#include "test_manager_tests.h"

int main(int argc, char** argv) {

    test_manager_initialize();

    // Register tests here
    test_manager_tests_register_all_tests();

    test_manager_run_tests();
    test_manager_shutdown();
    return 0;
}