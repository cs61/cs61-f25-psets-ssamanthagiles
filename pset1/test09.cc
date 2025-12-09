#include "m61.hh"
#include <cstdio>
#include <cassert>
#include <cstring>
// Check that null pointers can be freed.

int main() {
    void* p = m61_malloc(10); // Allocate 10 bytes to pointer p
    m61_free(nullptr); // Free a nullptr, which should do nothing to test that freeing nullptr is safe
    m61_free(p); // Free the pointer p
    m61_print_statistics(); // Print the memory statistics to check that the active allocation count is 0 and total allocation count is 1
}

//! alloc count: active          0   total          1   fail          0
//! alloc size:  active        ???   total         10   fail          0
