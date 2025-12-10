#include "m61.hh"
#include <cstdio>


// Testing 3 Block Coalescing Order ! (Free Middle First)
int main() {
    // Allocate three blocks in a row.
    void* a = malloc(1000);// Block A
    void* b = malloc(500); // Block B (middle)
    void* c = malloc(1200); // Block C

    // Free in a different order than staff tests.
    free(b); // free middle first
    free(a); // free left
    free(c); // free right 
    
    // Now malloc a large block that fits only if coalescing worked.
    void* big = malloc(1000 + 500 + 1200 - 100); 

    if (!big) {
        printf("FAIL: allocator did not coalesce all three blocks\n");
        return 0;
    }

    // No need for printed output 
    return 0;
}
