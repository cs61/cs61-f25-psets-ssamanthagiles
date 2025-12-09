#include "m61.hh"
#include <cstdio>
#include <cassert>
#include <cinttypes>
// Check failed allocation size statistic.

int main() {
    void* ptrs[10]; // Array to hold pointers that are returned by m61_malloc
    for (int i = 0; i != 10; ++i) { // Loop 10 times
        ptrs[i] = m61_malloc(i + 1); // Hold the pointers returned by m61_malloc and increase the size of each allocation by 1 byte
    }
    for (int i = 0; i != 5; ++i) { // Loop five times
        m61_free(ptrs[i]); // Free the first five allocations 
    }
    size_t very_large_size = SIZE_MAX - 200; // A very large size that is likely to exceed available memory
    void* garbage = m61_malloc(very_large_size); // Attempt to allocate a very large block of memory, which should fail
    assert(!garbage); // Ensure that the allocation failed and returned nullptr
    m61_print_statistics();
}

// The text within ??{...}?? pairs is a REGULAR EXPRESSION.
// (Some sites about regular expressions:
//  http://www.lornajane.net/posts/2011/simple-regular-expressions-by-example
//  https://www.icewarp.com/support/online_help/203030104.htm
//  http://xkcd.com/208/
// Dig deeper into how regular expresisons are implemented:
//  http://swtch.com/~rsc/regexp/regexp1.html )
// This particular regular expression lets our check work correctly on both
// 32-bit and 64-bit architectures. It checks for a `fail_size` of either
// 2^32 - 201 or 2^64 - 201.

//! ???
//! alloc count: active          5   total         10   fail          1
//! alloc size:  active        ???   total         55   fail ??{4294967095|18446744073709551415}??
