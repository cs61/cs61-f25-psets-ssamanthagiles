// randseek61.cc
// performs random seeks and reads to stress-test 
// caching and position tracking.

#include "io61.hh"
#include <cstdio>
#include <vector>
#include <cstdlib>

int main() {
    const char* fname = "randseek.txt";

    // create predictable file content
    io61_file* w = io61_open_check(fname, O_WRONLY | O_CREAT | O_TRUNC);
    for (int i = 0; i < 50000; i++) {
        io61_writec(w, (unsigned char)(i % 256));
    }
    io61_close(w);

    io61_file* f = io61_open_check(fname, O_RDONLY);
    std::vector<unsigned char> buf(6000);

    // random seeks + random block reads
    for (int i = 0; i < 1000; i++) {
        int block = rand() % 5000 + 1;
        int offset = rand() % 50000;
        io61_seek(f, offset);
        io61_read(f, buf.data(), block);
    }

    io61_close(f);
    printf("random seek test finished.\n");
}
