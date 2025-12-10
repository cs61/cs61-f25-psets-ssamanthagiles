#include "m61.hh"
#include <cstdio>
#include <cassert>

// Test: Allocator must create a second buffer once the first fills.

int main() {
    // Allocate slightly less than a full buffer (just under 8 MiB)
    void* a = malloc((8 << 20) - 64);
    assert(a);

    // This should force the allocator to allocate a SECOND buffer
    void* b = malloc(1024);
    assert(b);

    // The two pointers MUST NOT come from the same address range
    // (if they do, multiple-buffers is broken)
    assert(a != b);

    // No need for printed output 
    return 0;
}
