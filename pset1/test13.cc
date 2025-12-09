#include "m61.hh"
#include <cstdio>
#include <cassert>
#include <cstring>
#include <cstdlib>
// Check heap_min and heap_max using m61_allocator.

int main() {
    uintptr_t heap_first = 0; // heap_first is the first address allocated on the heap
    uintptr_t heap_last = 0; // Last address allocated

    // The `allocator` object allocates arrays of bytes using
    // m61_malloc and m61_free.
    m61_allocator<char> allocator; // Creates allocator object for char type that calls from m61_allocator class in m61.hh
    for (int i = 0; i != 100; ++i) { // Loop 100 times
        size_t sz = rand() % 100; // sz is a random size between 0 and 99
        char* p = allocator.allocate(sz); // Allocate sz bytes using the allocator object and store the returned pointer in p
        if (!heap_first || heap_first > (uintptr_t) p) { // If heap_first is 0 or if p is smaller than heap_first
            heap_first = (uintptr_t) p; // Then update heap_first to be p that way it always holds the smallest address allocated
        }
        if (!heap_last || heap_last < (uintptr_t) p + sz) { // If heap_last is 0 or if the end of the allocated block (p + sz) is larger than heap_last
            heap_last = (uintptr_t) p + sz; // Then update heap_last to be the end of the allocated block to make sure it always holds the largest address allocated
        }
        allocator.deallocate(p, sz); // Free the pointer p using the allocator object created in m61_allocator class in m61.hh lines 66-68 that says to call m61_free
    }

    m61_statistics stat = m61_get_statistics(); // Declaring stat and setting it equal to the return value of m61_get_statistics()
    assert(stat.heap_min <= heap_first); // Assert (test) that heap_min is less than or equal to heap_first for stat variable
    assert(heap_last - 1 <= stat.heap_max); // Assert (test) that heap_last - 1 is less than or equal to heap_max for stat variable
}
