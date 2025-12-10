// interleave61.cc
// reads from three different files in an interleaved fashion
// basically a stress test for multiple file handles

#include "io61.hh"
#include <cstdio>

int main() {
    const char* f1name = "f1.txt";
    const char* f2name = "f2.txt";
    const char* f3name = "f3.txt";

    // create three files with simple patterns
    const char* names[3] = { f1name, f2name, f3name };
    for (int k = 0; k < 3; k++) {
        io61_file* w = io61_open_check(names[k], O_WRONLY | O_CREAT | O_TRUNC);
        for (int i = 0; i < 10000; i++) {
            io61_writec(w, (unsigned char)((i + k) % 256));
        }
        io61_close(w);
    }

    io61_file* f1 = io61_open_check(f1name, O_RDONLY);
    io61_file* f2 = io61_open_check(f2name, O_RDONLY);
    io61_file* f3 = io61_open_check(f3name, O_RDONLY);

    // cycle reads across all three files
    for (int i = 0; i < 5000; i++) {
        io61_readc(f1);
        io61_readc(f2);
        io61_readc(f3);
    }

    io61_close(f1);
    io61_close(f2);
    io61_close(f3);

    printf("interleave test finished.\n");
}
