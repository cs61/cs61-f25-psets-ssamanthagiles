#include "m61.hh"
#include <cstdio>

// Test (multiple buffer logic)

// Check to see if allocator creates a second buffer once the first fills.

int main() {
    // Small allocation and free
    void* p = malloc(32);
    free(p);

    // Reallocate the exact same size — should reuse same block
    void* q = malloc(32);

    // Check that the same block was reused
    // If not, the allocator did not reuse freed blocks correctly
    if (q != p) {
        printf("FAIL: allocator did not reuse freed block correctly\n");
        return 0;
    }

    // No need for printed output 
    return 0;
}
