// misalignwrite61.cc
// writes blocks of unusual sizes to check write buffering 
// and flush behavior

#include "io61.hh"
#include <cstdio>
#include <cstring>

int main() {
    const char* fname = "misalign.txt";

    // use plain arrays instead of std::vector for compatibility
    size_t sizes[] = {7, 13, 4095, 2, 10000};
    int sizes_len = sizeof(sizes) / sizeof(sizes[0]);

    unsigned char pattern[20000];
    for (size_t i = 0; i < 20000; i++) {
        pattern[i] = (unsigned char)(i % 256);
    }

    // write misaligned block sizes
    io61_file* w = io61_open_check(fname, O_WRONLY | O_CREAT | O_TRUNC);

    size_t pos = 0;
    for (int i = 0; i < sizes_len; i++) {
        io61_write(w, pattern + pos, sizes[i]);
        pos += sizes[i];
    }

    io61_close(w);

    // read back and verify
    unsigned char* out = new unsigned char[pos];
    io61_file* r = io61_open_check(fname, O_RDONLY);
    io61_read(r, out, pos);
    io61_close(r);

    if (memcmp(out, pattern, pos) == 0) {
        printf("misalignment test passed.\n");
    } else {
        printf("misalignment test failed.\n");
    }

    delete[] out;
}
