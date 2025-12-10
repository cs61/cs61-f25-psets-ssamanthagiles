#include "m61.hh"
#include <cassert>
#include <cstdio>

// multiple buffers test

int main() {
    // allocate small block from buffer 1
    void* a = malloc(1); 
    assert(a);

    // exhaust buffer 1
    // 7 is my lucky number 
    for (int i = 0; i < 7000; i++) {
        if (!malloc(4096)) break;
    }

    // now we are allocating from buffer 2
    void* b = malloc(1);
    assert(b);

    free(a);
    free(b);

    // this malloc fails if you merged free blocks across buffers
    assert(malloc(1));

    // No need for printed output 
    return 0;
}
