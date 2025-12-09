#include "m61.hh"
#include <cstdio>
#include <cassert>
#include <vector>
// Demonstrate destructors.

void f() {
    std::vector<int, m61_allocator<int>> v; // Creates vector of integers caled v (uses the same m61_allocator as in test16)
    for (int i = 0; i != 100; ++i) { // Loop 100 times
        v.push_back(i); // Push the value of i to the back of v
    }
    // `v` has automatic lifetime, so it is destroyed here.
    // When f() ends, v’s destructor calls m61_free automatically
}

int main() { 
    f(); // Call function f() above that creates vector v and pushes back 100 integers to it
    m61_print_statistics(); 
    m61_print_leak_report(); // Print the leak report to make sure there are no memory leaks
}

//! alloc count: active          0   total    ??>=1??   fail          0
//! alloc size:  active        ???   total  ??>=400??   fail          0
