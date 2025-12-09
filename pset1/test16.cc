#include "m61.hh"
#include <cstdio>
#include <cassert>
#include <vector>
// Check that C++ library functions can call your allocator.
// (They do not provide line number information.)

int main() {
    // The `m61_allocator<int>` argument tells the C++ standard library
    // to allocate `v`’s memory using `m61_malloc/free`.
    std::vector<int, m61_allocator<int>> v;  // Creates vector of integers called v that uses m61_allocator class in m61.hh to allocate memory using m61_malloc and m61_free
    for (int i = 0; i != 100; ++i) { // Loop 100 times 
        v.push_back(i); // Push the value of i to the back of v
    }
    m61_print_statistics(); // Print the memory statistics to check that the active allocation count is 1 and total allocation count is >= 1
}

//! alloc count: active          1   total    ??>=1??   fail          0
//! alloc size:  active        ???   total  ??>=400??   fail        ???
