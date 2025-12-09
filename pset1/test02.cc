#include "m61.hh"
#include <cstdio>
// Check total allocation count statistic.

int main() {
    for (int i = 0; i != 10; ++i) { //Run loop 10 times
        (void) m61_malloc(1); // Allocate 1 byte each time in the m61_malloc function
    }
    m61_print_statistics(); // Print the memory statistics of the updated total allocation count statistic
}

// In expected output, "???" can match any number of characters.

//! alloc count: active        ???   total         10   fail        ???
//! alloc size:  active        ???   total        ???   fail        ???
