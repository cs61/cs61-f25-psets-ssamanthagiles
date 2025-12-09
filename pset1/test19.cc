#include "m61.hh"
#include <cstdio>
#include <cassert>
#include <cstring>
// Check diabolical m61_calloc.
// Making sure m61_calloc can safely test overflow safely

int main() {
    size_t very_large = (size_t) -1 / 8 + 2; // Create very_large of type size_t equal to (size_t) -1 / 8 + 2
    void* p = m61_calloc(very_large, 16); // In pointer p, allocate very_large elements of size 16 bytes using m61_calloc
    assert(p == nullptr); // Assert (test) that the allocation failed and returned nullptr

    void* p2 = m61_calloc(16, very_large); // In pointer p2, allocate 16 elements of size very_large bytes using m61_calloc
    assert(p2 == nullptr); // Assert (test) that the allocation failed and returned nullptr

    m61_print_statistics();
}

//! alloc count: active          0   total          0   fail          2
//! alloc size:  active          0   total          0   fail        ???
