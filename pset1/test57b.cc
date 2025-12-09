#include "m61.hh"
#include <cstdio>

int main() {
    // allocate 10 bytes
    char* p = (char*) malloc(10);
    // handle if malloc fails
    if (!p) {
        printf("FAIL: malloc returned null\n");
        return 0;
    }
    // fill all 10 bytes 
    for (int i = 0; i < 10; i++) {
        // starting from 0 to 9
        p[i] = '0' + i;
    }

    // now realloc to shrink to 5 bytes
    char* q = (char*) realloc(p, 5);

    // handle if realloc fails
    if (!q) {
        printf("FAIL: realloc returned null\n");
        return 0;
    }

    // shrinking should not move the block
    // verify with this if statement 
    if (q != p) {
        printf("FAIL: shrink moved pointer\n");
        return 0;
    }

    // first 5 chars should remain
    for (int i = 0; i < 5; i++) {
        if (q[i] != '0' + i) {
            printf("FAIL: incorrect contents after shrink\n");
            return 0;
        }
    }

    free(q);
    return 0;  // all good, silent
}
