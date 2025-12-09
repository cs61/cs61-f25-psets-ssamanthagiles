#include "m61.hh"
#include <cstdio>
#include <cassert>
#include <vector>
#include <random>
// Check for memory reuse: up to 100 variable-sized active allocations.

int main() {
    std::default_random_engine randomness(std::random_device{}()); // Initialize random # generator  

    const size_t nmax = 100; // Max num of active allocations
    char* ptrs[nmax]; // Array to store pointers to allocated blocks
    size_t sizes[nmax]; // Array to store the size of each allocated block 
    size_t n = 0; // Variable to hold current # of active allocations

    // 5000 times, allocate or free
    for (int i = 0; i != 10000; ++i) { // Repeat allocate or free opeartions 10000 times
        // uniform_int is a function that generates a random integer in a specified range (has equal probability of being picked)
        // Generate a random integer between 0 and nmax^2 -1
        // n^2 is the threshold to free memory
        if (uniform_int(size_t(0), nmax * nmax - 1, randomness) < n * n) {
            assert(n > 0); // Ensure at least one allocation exists
            size_t j = uniform_int(size_t(0), n - 1, randomness); // Pick random index (0 to n - 1) of the active allocations to free
            m61_free(ptrs[j]); // Free that block 
            --n; // Decrease count of active allocations
            ptrs[j] = ptrs[n]; // Replace freed slot with the last active pointer
            sizes[j] = sizes[n]; // Replace freed slot with corresponding last pointer 
        } else {
            assert(n < nmax); // Assert (check) that the current number of active allocations is less than the max number of active allocations (100)
            size_t size = uniform_int(1, 4000, randomness); // Pick a random size between 1 and 4000 bytes
            char* ptr = (char*) m61_malloc(size); // Allocate a memory block of 'size' bytes and store pointer in ptr
            assert(ptr); // Assert (check) that the allocation was successful
            for (size_t k = 0; k != n; ++k) { 
                assert(ptrs[k] + sizes[k] <= ptr || ptr + size <= ptrs[k]); // Assert that newly allocated block of memory does not overlap with any previous
            }
            ptrs[n] = ptr; // Store pointer to new block in ptrs array
            sizes[n] = size; // Store size of new block in sizes array
            ++n; // Increment number of active allocations
        }
    }

    // clean up
    for (size_t j = 0; j != n; ++j) {
        m61_free(ptrs[j]); // Freeing any remaining active allocations 
    }

    m61_print_statistics();
}

//! alloc count: active          0   total        ???   fail          0
//! alloc size:  active        ???   total        ???   fail          0
