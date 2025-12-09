#include "m61.hh"
#include <cstdio>
// Check total allocation size statistic.

int main() {
    void* ptrs[10]; // Array to hold pointers that are returned by m61_malloc
    for (int i = 0; i != 10; ++i) { // Loop 10 times
        ptrs[i] = m61_malloc(i + 1); // Hold the pointers returned by m61_malloc and increase the size of each allocation by 1 byte
    }
    for (int i = 0; i != 5; ++i) { // Loop five times
        m61_free(ptrs[i]); // Free the first five allocations
    } 
    m61_print_statistics();
}

//! alloc count: active          5   total         10   fail        ???
//! alloc size:  active        ???   total         55   fail        ???
