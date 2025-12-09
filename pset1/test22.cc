#include "m61.hh"
#include <cstdio>
// Check active allocation size statistic.

int main() {
    void* ptrs[10]; // Array called ptrs to store 10 pointers  
    for (int i = 0; i != 10; ++i) { // Loop 10 times
        ptrs[i] = m61_malloc(i + 1); // Store each allocated pointer in ptrs[i]
    }
    for (int i = 0; i != 5; ++i) { // Loop 5 times
        m61_free(ptrs[i]); // Free memory at ptrs[i]
    }
    m61_print_statistics();
}

//! alloc count: active          5   total         10   fail        ???
//! alloc size:  active         40   total         55   fail        ???
