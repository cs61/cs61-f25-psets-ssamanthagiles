#include "m61.hh"
#include <cstdio>
#include <cstring>
#include <cassert>
#include <vector>
// Check that freed memory may be immediately reused.

static void check_contents(unsigned char* p, unsigned char ch) {
    unsigned char buf[10];
    memset(buf, ch, 10);
    assert(memcmp(p, buf, 10) == 0);
}

int main() {
    const size_t nmax = 10000;
    unsigned char* ptrs[nmax];
    size_t n = 0;
    // Allocating whole heap (8 mb buffer) 
    // Calling malloc repeatedly to allocate 850 bytes each time at which point buffer will be full
    while (n != nmax) {
        ptrs[n] = (unsigned char*) m61_malloc(850);
        if (!ptrs[n]) {
            break;
        } 
        memset(ptrs[n], n & 255, 10);
        ++n;
    }

    if (n > 0) { // Succesfully allocated at least one block
        size_t f = n / 2; // Half as many allocations
        check_contents(ptrs[f], f & 255); // ptrs[f] holds all the malloc calls, middle allocation
        m61_free(ptrs[f]); // Freeing the block

        // Making sure we can reuse freed blocks 
        ptrs[f] = (unsigned char*) m61_malloc(850); // And then mallocing again
        assert(ptrs[f]); // Checking that the allcoation worked
        memset(ptrs[f], f & 255, 10);
    }

    for (size_t i = 0; i != n; ++i) {
        if (ptrs[i]) {
            check_contents(ptrs[i], i & 255);
        }
        m61_free(ptrs[i]);
    }
    m61_print_statistics();
}

//! alloc count: active          0   total ??>=8000??   fail  ??{0|1}??
//! alloc size:  active        ???   total ??>=6800000??   fail ??{0|850}??
