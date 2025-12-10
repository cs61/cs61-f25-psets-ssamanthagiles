#include "m61.hh"
#include <cstdio>

int main() {
    // reallocate 20 bytes
    char* p = (char*) m61_realloc(nullptr, 20, __FILE__, __LINE__);

    // handle errors
    if (!p) {
        printf("FAIL: m61_realloc returned null\n");
        return 0;
    }

    // free with m61_free
    m61_free(p);

    // success no output
    return 0;
}
