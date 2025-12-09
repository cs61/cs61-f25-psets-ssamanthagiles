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
        m61_free(ptrs[i]); // 
    }
    size_t very_large_size = SIZE_MAX; // Creates very_large_size equal to SIZE_MAX 
    void* garbage = m61_malloc(very_large_size); // Attempt to allocate a very large block of memory into pointer garbage 
    assert(garbage == nullptr); // Assert (test) that the allocation failed and returned nullptr
    m61_print_statistics(); 
}

//! alloc count: active          5   total         10   fail          1
//! alloc size:  active        ???   total         55   fail ??{4294967295|18446744073709551615}??
