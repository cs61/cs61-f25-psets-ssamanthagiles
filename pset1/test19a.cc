#include "m61.hh"
#include <cstdio>
#include <cassert>
#include <cstring>
// m61_calloc with 0 count and/or size should be like m61_malloc(0).
// Count = element count , size = element size (bytes)

int main() {
    void* p = m61_calloc(100, 0); // Creating a pointer p by calling m61_calloc with count 100 and size 0
    m61_free(p); // Freeing the memory that was preivously allocated to pointer p
    p = m61_calloc(0, 100);   // Same a sline 9 but count = 0 and size = 100
    m61_free(p); // Freeing pointer p again
    p = m61_calloc(0, 0); // Now assiging count and size to 0
    m61_free(p); // Freeing pointer p again
    m61_print_statistics(); // Printing memory allocation statistics 
}

//! alloc count: active          0   total  ??{0|3}??   fail          0
//! alloc size:  active          0   total          0   fail        ???
