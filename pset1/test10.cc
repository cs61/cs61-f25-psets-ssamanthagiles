#include "m61.hh"
#include <cstdio>
#include <cassert>
#include <cstring>
// Check heap_min and heap_max, simple case.

int main() {
    char* p = (char*) m61_malloc(10); // Allocate 10 bytes to pointer p
    
    m61_statistics stat = m61_get_statistics(); // Get current memory statistics
    assert((uintptr_t) p >= stat.heap_min); // Ensure that the address of p is greater than or equal to heap_min
    assert((uintptr_t) p + 9 <= stat.heap_max); // Ensure that the end address of the allocated block (p + 9) is less than or equal to heap_max

    m61_free(p); // Free the pointer p
}
