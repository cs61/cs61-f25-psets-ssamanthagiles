#include "m61.hh"
#include <cstdio>
// The distance from heap_min to heap_max should be big enough to hold
// any block that was allocated.

int main() {
    void *p = m61_malloc(17); // Allocate 17 bytes to pointer p
    assert(p); // Ensure that the allocation was successful through the assert statement that checks if p is not nullptr
    m61_free(p); // Free the pointer p

    p = m61_malloc(19); // Allocate 19 bytes to pointer p
    assert(p); // Ensure that the allocation was successful
    m61_free(p); // Free the pointer p

    m61_statistics stats = m61_get_statistics(); // Get current memory statistics
    assert(stats.heap_max - stats.heap_min >= 18); // Ensure that the distance from heap_min to heap_max is at least 18 bytes because the largest allocation was 19 bytes
    //Largest memory allocation was 19 bytes, so the distance between heap_min and heap_max should be at least 18 bytes (19 - 1 = 18)
}
