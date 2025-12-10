// pingpong61.cc
// alternate very small and very large reads to check cache behavior

#include "io61.hh"
#include <vector>
#include <cstdio>

int main() {
    const char* fname = "pingpong.txt";

    // create a predictable file
    io61_file* w = io61_open_check(fname, O_WRONLY | O_CREAT | O_TRUNC);
    for (int i = 0; i < 20000; i++) {
        io61_writec(w, (unsigned char)(i % 256));
    }
    io61_close(w);

    // alternate tiny and large reads
    io61_file* f = io61_open_check(fname, O_RDONLY);
    std::vector<unsigned char> buf(9000);

    while (true) {
        int r1 = io61_read(f, buf.data(), 1);
        int r2 = io61_read(f, buf.data(), 4096);
        if (r1 + r2 == 0) break;
    }

    io61_close(f);
    printf("pingpong test finished.\n");
}
