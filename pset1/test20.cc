#include "m61.hh"
#include <cstdio>
#include <cassert>
#include <cstring>
// Check second diabolical m61_calloc.

struct large_struct {
    short a[0x80000001UL]; // Creating a struct called large_struct with an array of shorts of size 0x80000001UL
};

int main() {
    void* p = m61_calloc(0x100000001UL, sizeof(large_struct)); // Allocating memory in pointer p using m61_calloc that is the count of 0x100000001UL and size of large_struct
    // Should fail and return null pointer p
    assert(p == nullptr);
    m61_print_statistics();
}

//! alloc count: active          0   total          0   fail          1
//! alloc size:  active          0   total          0   fail        ???
