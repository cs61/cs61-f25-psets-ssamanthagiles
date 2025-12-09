#include "m61.hh"
#include <cstdio>
#include <cassert>
#include <cstring>
#include <random>
// Check that m61_calloc works when called ~500 times.

int main() {
    std::default_random_engine randomness(std::random_device{}()); // From C++ <random> library, a random number that is a type for an engine that produces pseudo-random numbers

    constexpr int nptrs = 5; // Creates constant integer nptrs equal to 5
    char* ptrs[nptrs] = {nullptr, nullptr, nullptr, nullptr, nullptr}; // Array of 5 char pointers initialized to nullptr called ptrs

    for (int round = 0; round != 1000; ++round) { // Loop 1000 times
        int index = uniform_int(0, nptrs - 1, randomness); // index is a random integer between 0 and nptrs - 1 so 4 in order to access all elements of the ptrs array which only holds 5 elements
        if (!ptrs[index]) { // If ptrs[index] is nullptr
            // Allocate a new randomly-sized block of memory
            size_t size = uniform_int(1, 2000, randomness); // Then size is a random integer between 1 and 2000
            char* p = (char*) m61_calloc(size, 1); // Pointer p is set equal to the return value of m61_calloc which allocates size (declared above) bytes and each element is 1 byte
            assert(p != nullptr); // Assert (test) that the allocation works and checks if p is not nullptr
            // check contents
            size_t i = 0; // Create size_t variable i and set it equal to 0
            while (i != size && p[i] == 0) { // Loop while i is not equal to size AND the ith byte of p is equal to 0
                ++i; // Increment i while above condition is met making sure the entire allocated block is 0
            }
            assert(i == size);  // Assert (test) that i is equal to size meaning the entire allocated block is 0
            // set to non-zero contents and save
            memset(p, 'A', size); // Set all bytes of p to 'A' using memset and size is the number of bytes to be set to 'A'
            ptrs[index] = p; // Save pointer p to ptrs[index]

        } else {
            // Free previously-allocated block
            m61_free(ptrs[index]);
            ptrs[index] = nullptr; // Set ptrs[index] back to nullptr
        }
    }

    for (int i = 0; i != nptrs; ++i) { // Loop through all elements of the ptrs array if not nullptr
        m61_free(ptrs[i]); // Free ptrs[i]
    }

    m61_print_statistics(); // Print the memory statistics to check that the active allocation count is 0 and total allocation count is >= 500
}

//! alloc count: active          0   total  ??>=500??   fail          0
//! alloc size:  active        ???   total        ???   fail          0
