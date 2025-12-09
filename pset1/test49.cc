#include "m61.hh"
#include <cstdio>
#include <cassert>
#include <cstring>
// Check detection of boundary write error after aligned block.

int main() {
    int* array = (int*) m61_malloc(16 * sizeof(int));
    for (int i = 0; i != 18; ++i) {
        array[i] = 0;
    }
    m61_free(array);
    m61_print_statistics();
}

//! MEMORY BUG???: detected wild write during free of pointer ???
//! ???
//!!ABORT
