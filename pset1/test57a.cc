#include "m61.hh"
#include <cstdio>
#include "m61.hh"
#include <cstdio>
#include <cstring>


int main() {
    // use malloc to allocate space for 1 integer (4 bytes)
    int* p = (int*) malloc(sizeof(int));
    // handle if allocation fails
    if (!p) {
        printf("FAIL: malloc returned null\n");
        return 0;
    }

    // store a known integer value in allocated space
    *p = 12345;

    // use realloc to resize the allocated space to hold 2 integers (8 bytes)
    int* q = (int*) realloc(p, 2 * sizeof(int));
    // handle if allocation fails
    if (!q) {
        printf("FAIL: realloc returned null\n");
        return 0;
    }

    // check that the original data is still intact
    if (q[0] != 12345) {
        // if not then was not allocated correctly
        printf("FAIL: old data not preserved\n");
        return 0;
    }
    
    // store a new integer value in the newly allocated space
    q[1] = 54321;
    if (q[1] != 54321) {
        // check if allocation was successful
        printf("FAIL: new region not writable\n");
        return 0;
    }

    // free allocated memory
    free(q);
    return 0;
}
