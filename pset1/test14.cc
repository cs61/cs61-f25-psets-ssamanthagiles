#include "m61.hh"
#include <cstdio>
#include <cassert>
#include <cstring>
// Check that m61_calloc zeroes the returned allocation.

const char data[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; // Array of 10, 0 bytes called data

int main() {
    char* p = (char*) m61_malloc(10); // Allocate 10 bytes to pointer p
    assert(p != nullptr); // Assert (test) that the allocation worked through the assert statement that checks if p is not nullptr
    memset(p, 255, 10); // Set all 10 bytes of p to 255 using memset
    m61_free(p); // Free the pointer p
    p = (char*) m61_calloc(10, 1); // Allocate 10 bytes to pointer p using m61_calloc which should set all bytes to 0 and size of each element is 1 byte
    assert(p != nullptr); // Assert (test) that the allocation works by checking if p is not nullptr
    assert(memcmp(data, p, 10) == 0); // Assert (test) that the first 10 bytes of p are all zero by comparing it to the data array (that holds 10, 0 bytes) using memcmp which only passes if the two memory blocks are identical
    m61_print_statistics(); // Print the memory statistics to check that the active allocation count is 1 and total allocation count is 2
}

//! alloc count: active          1   total          2   fail          0
//! alloc size:  active        ???   total         20   fail          0
