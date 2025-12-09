#include "m61.hh"
#include <cstdio>
#include <cassert>
#include <cstring>
#include <cinttypes>
#include <cstddef>
// Check for correct allocation alignment.

// Double requirements for alignment = 8
// Unsigned long long requirements for alignment = 8
// max_align_t requirements for alignment = 16 

int main() {
    double* ptr = (double*) m61_malloc(sizeof(double)); // Allocate memory for a double to ptr ()
    assert((uintptr_t) ptr % alignof(double) == 0); // Checking alignment for double (2 doesn't have a remainder bc of mod)
    assert((uintptr_t) ptr % alignof(unsigned long long) == 0); // Checking alignment for unsigned long long
    //Is it okay if I create a new pointer for max_align_t? (ptr3)
    std::max_align_t* ptr3 = (std::max_align_t*) m61_malloc(sizeof(std::max_align_t)); // Allocate memory for a max_align_t to ptr3
    assert((uintptr_t) ptr3 % alignof(std::max_align_t) == 0); // Checking alignment for max_align_t

    char* ptr2 = (char*) m61_malloc(1);
    assert((uintptr_t) ptr2 % alignof(double) == 0);
    assert((uintptr_t) ptr2 % alignof(unsigned long long) == 0);
    assert((uintptr_t) ptr2 % alignof(std::max_align_t) == 0);

    m61_free(ptr);
    m61_free(ptr2);
}
