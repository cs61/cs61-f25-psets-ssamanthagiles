#include "m61.hh"
#include <cstdio>
// Check active allocation count statistic.

int main() {
    void* ptrs[10]; // Array to hold pointers that are returned by m61_malloc
    for (int i = 0; i != 10; ++i) { // Loop 10 times
        ptrs[i] = m61_malloc(i + 1);  //Allocate 10 blocks of increasing size (ptrs[0] = 1 byte, ptrs[1] = 2 bytes, ..., ptrs[9] = 10 bytes)
    }
    for (int i = 0; i != 5; ++i) { // Free the first 5 allocations (ptrs[0] to ptrs[4])
        m61_free(ptrs[i]); 
    }
    m61_print_statistics(); // Print the memory statistics to check active allocation count
}

//! alloc count: active          5   total         10   fail        ???
//! alloc size:  active        ???   total        ???   fail        ???
