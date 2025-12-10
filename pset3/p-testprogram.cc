#include "u-lib.hh"

// p-testprogram.cc

void process_main() {

    console_printf("testprogram start\n");

    // test uptime
    long up1 = sys_uptime();
    console_printf("uptime: %ld\n", up1);

    // test sleep
    console_printf("sleeping for 50 ticks\n");
    sys_sleep(50);
    long up2 = sys_uptime();
    console_printf("uptime after sleep: %ld\n", up2);

    // test random
    console_printf("random numbers:\n");
    for (int i = 0; i < 5; i++) {
        console_printf("%ld\n", sys_random());
    }

    // test page_alloc & page_free
    void* p = (void*) 0x100000;  // valid user VA, page aligned

    int a = sys_page_alloc(p);
    console_printf("page_alloc returned %d for %p\n", a, p);

    if (a == 0) {
        int r = sys_page_free(p);
        console_printf("page_free returned %d\n", r);

        int r2 = sys_page_free(p);
        console_printf("second free returned %d\n", r2);
    }

    // test kill
    console_printf("trying kill on pid 2\n");
    int k = sys_kill(2);
    console_printf("kill returned %d\n", k);

    console_printf("testprogram end\n");

    // prevent overlap with kernel memory dump
    sys_sleep(5);

    sys_exit();
}
