#include "m61.hh"
#include <cstdio>
#include <cassert>
#include <cinttypes>
// Check that certain non-failures aren't considered failures.

int main() {
    void* a = m61_malloc(0); // Allocate 0 bytes to m61_malloc 
    m61_free(a); // Store the returned pointer in variable a 
    m61_free(nullptr); // Free a nullptr
    m61_print_statistics();
}

//! alloc count: active          0   total  ??{0|1}??   fail          0
//! alloc size:  active        ???   total          0   fail          0
