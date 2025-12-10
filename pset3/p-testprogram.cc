#include "u-lib.hh"

// test program for basic system call functionality
// uses assertions to check correctness and edge cases

void process_main() {

    console_printf("testprogram start\n");

    // test uptime
    long up1 = sys_uptime();
    assert(up1 >= 0);

    long up1b = sys_uptime();
    assert(up1b >= up1);

    // test sleep
    sys_sleep(50);
    long up2 = sys_uptime();
    assert(up2 - up1 >= 50);

    // test random
    long rvals[5];
    for (int i = 0; i < 5; i++) {
        rvals[i] = sys_random();
    }

    bool changed = false;
    for (int i = 1; i < 5; i++) {
        if (rvals[i] != rvals[0]) {
            changed = true;
            break;
        }
    }
    assert(changed);

    // test page_alloc and page_free
    void* p = (void*) 0x100000;  // valid, page-aligned user address

    int a1 = sys_page_alloc(p);
    assert(a1 == 0);

    int a2 = sys_page_alloc(p);  // should fail
    assert(a2 < 0);

    int fr1 = sys_page_free(p);
    assert(fr1 == 0);

    int fr2 = sys_page_free(p);  // double free should fail
    assert(fr2 < 0);

    int fr3 = sys_page_free((void*)0x12345);  // invalid address
    assert(fr3 < 0);

    // test kill
    int k1 = sys_kill(9999);  // nonexistent pid
    assert(k1 < 0);

    int k2 = sys_kill(2);  // expected failure if pid 2 not active
    assert(k2 < 0);

    int me = sys_getpid();
    int k3 = sys_kill(me);  // cannot kill current process
    assert(k3 < 0);

    console_printf("all tests passed\n");

    sys_sleep(5);
    sys_exit();
}
