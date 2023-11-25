#include <defines.h>
#include <etmemory.h>

#include <stdio.h>

int main(int argc, char** argv) {
    memory_init();

    u32* testing_allocation = (u32*)etallocate(sizeof(u32) * 10, MEMORY_TAG_STRING);
    printf("%llu\n", get_total_allocs());

    etfree(testing_allocation, sizeof(u32) * 10, MEMORY_TAG_STRING);
    printf("Hello World.\n");
}