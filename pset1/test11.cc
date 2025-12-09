#include "m61.hh"
#include <cstdio>
#include <cassert>
#include <cstring>
#include <cstdlib>
// Check heap_min and heap_max with more allocations.

int main() {
    uintptr_t heap_first = 0; // heap_first is the first address allocated on the heap
    uintptr_t heap_last = 0; // Last address allocated
    for (int i = 0; i != 100; ++i) { // Loop 100 times
        size_t sz = rand() % 100; // sz is a random size between 0 and 99
        char* p = (char*) m61_malloc(sz); // Allocate sz bytes in m61_malloc and store the returned pointer in p
        if (!heap_first || heap_first > (uintptr_t) p) { // Condition: If heap_first is 0 or if p is smaller than heap_first
            heap_first = (uintptr_t) p; // Then update heap_first to be p ensuring it always holds the smallest address allocated
        }
        if (!heap_last || heap_last < (uintptr_t) p + sz) { // Condition: If heap_last is 0 or if the end of the allocated block (p + sz) is larger than heap_last
            heap_last = (uintptr_t) p + sz; // Then update heap_last to be the end of the allocated block to make sure it always holds the largest address allocated
        }
        m61_free(p); // Free the pointer p
    }

    m61_statistics stat = m61_get_statistics(); // Declaring stat and setting it equal to the return value of m61_get_statistics()
    // Check that the recorded heap_min and heap_max are within bounds
    assert(stat.heap_min <= heap_first); //
    assert(heap_last - 1 <= stat.heap_max);
}
