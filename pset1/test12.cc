#include "m61.hh"
#include <cstdio>
#include <cassert>
#include <cstring>
#include <cstdlib>
// Check that heap_min and heap_max do not overlap stack or globals.

static int global; // Creating int variable global

// Testing to see if random memory allocations overlap with stack or global variables
int main() { 
    for (int i = 0; i != 100; ++i) { // For loop that runs 100 times
        size_t sz = rand() % 100; // sz is a random size between 0 and 99
        char* p = (char*) m61_malloc(sz); // Store the returned pointer of m61_malloc in p
        m61_free(p); // Free the pointer p
    }
    m61_statistics stat = m61_get_statistics(); // After all 100 iterations, get the memory statistics and store it in stat

    union { // Create a union that is basically a struct but instead  all components share the same memory location
        uintptr_t addr; // addr is the address 
        int* iptr; // iptr is a pointer to an int
        m61_statistics* statptr; // statptr is a pointer to the m61_statistics struct
        int (*mainptr)(); // mainptr is a pointer to the main function
    } x; // Declare a variable x of the union type 
    x.iptr = &global; // Set iptr to the address of the global variable created in line 8 
    assert(x.addr + sizeof(int) < stat.heap_min || x.addr >= stat.heap_max); // Assert (test) that the address of global + sizeof(int) which is 4 bytes is either less than heap_min or greater than or equal to heap_max
    x.statptr = &stat; // Set statptr to the address of the stat variable created in line 20
    assert(x.addr + sizeof(int) < stat.heap_min || x.addr >= stat.heap_max); // Assert (test) that the address of stat (just changed due to union) + sizeof(int) which is 4 bytes is either less than heap_min or greater than or equal to heap_max
    x.mainptr = &main; // Set mainptr to the address of the main function
    assert(x.addr + sizeof(int) < stat.heap_min || x.addr >= stat.heap_max); // Assert (test) that the address of main (just changed due to union) + sizeof(int) which is 4 bytes is either less than heap_min or greater than or equal to heap_max
}
