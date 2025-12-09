#include "m61.hh"
#include <cstdio>
#include <cassert>
#include <cstring>
#include <random>
// Check 100 random diabolical m61_calloc arguments. (Extreme test cases)

int main() {
    std::default_random_engine randomness(std::random_device{}()); // Creating a default random engine object of class std:: default random_engine and initalizing it with a random device object

    void* success = m61_calloc(0x1000, 2); // Allocating memory in pointer success using m61_calloc (count = 0x 1000, size = 2)

    for (int i = 0; i != 100; ++i) { // Looping 100 times
        size_t a = uniform_int(size_t(0), size_t(0x2000000), randomness) * 16;  // size_t type is an unsigned integer type, and is used to create variable a that is a random integer between 0 and 0x2000000 multiplied by 16. Randomness is the random engine object created above
        size_t b = size_t(-1) / a; // Creating variable b taht is the maximum value of size_t divided by a
        b += uniform_int(size_t(1), size_t(0x20000000) / a, randomness); // Incrementing b by a random integer that has a size of size_t between 1 and 0x20000000 divided by a
        void* p = m61_calloc(a, b); //Creating a pointer p that allocated memory using m61_calloc with count a and size b
        assert(p == nullptr); // This should fail and return null pointer p, using assert to check
    }

    m61_free(success); // Freeing the memroy allcoated to pointer success using m61_free
    m61_print_statistics();
}

//! alloc count: active          0   total          1   fail        100
//! alloc size:  active        ???   total       8192   fail        ???
