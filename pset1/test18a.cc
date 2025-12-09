#include "m61.hh"
#include <cstdio>
#include <cassert>
// Check diabolical failed allocation.

int main() {
    void* ptrs[10]; // Array to hold 10 pointers
    for (int i = 0; i != 10; ++i) { // Loop 10 times
        ptrs[i] = m61_malloc(i + 1); // Hold the pointers returned by m61_malloc (through the statement ptrs[i]) and increase the size of each allocation by 1 byte
    }
    for (int i = 0; i != 5; ++i) { // Loop to free the first five allocations
        m61_free(ptrs[i]);
    }
    for (size_t delta = 1; delta != 9; ++delta) { // Create delta of type size_t and loop while delta is not equal to 9, increasing delta by 1 each time
        size_t very_large_size = SIZE_MAX - delta; // Creates very_large_size of type size_t equal to SIZE_MAX - delta
        void* garbage = m61_malloc(very_large_size); // Create a pointer called garbage that allocates the size very_large_size into m61_malloc
        assert(garbage == nullptr); // Assert (test) that the allocation failed and returned nullptr
    }
    m61_print_statistics();
}

//! alloc count: active          5   total         10   fail          8
//! alloc size:  active        ???   total         55   fail        ???
