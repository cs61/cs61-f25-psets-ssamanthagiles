#include "m61.hh"
#include <cstdio>
// Check that simultaneously-active allocations have different addresses.

int main() {
    void* ptrs[10]; // Array to hold pointers that are returned by m61_malloc
    for (int i = 0; i != 10; ++i) { // Again, loop 10 times 
        ptrs[i] = m61_malloc(1); //Asks for 1 byte of memory and stores the returned pointer in the array
        // Check that the new pointer is different from all previous pointers (j != i)
        for (int j = 0; j != i; ++j) {
            assert(ptrs[i] != ptrs[j]); // Assert that the current pointer is not equal to any previous pointer
        }
    }
    m61_print_statistics();
}

//! alloc count: active        ???   total         10   fail        ???
//! alloc size:  active        ???   total        ???   fail        ???
