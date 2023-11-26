#include <defines.h>
#include <engine.h>
#include <logger.h>

#include <stdio.h>

int main(int argc, char** argv) {
    engine_initialize();
    engine_run();
    engine_shutdown();
}