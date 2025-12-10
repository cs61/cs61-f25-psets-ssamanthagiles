// zigzagseek61.cc
// seeks around the file in different directions to check 
// if the cache resets correctly

#include "io61.hh"
#include <vector>
#include <cstdio>

int main() {
    const char* fname = "zigzag.txt";

    // create a predictable file
    io61_file* w = io61_open_check(fname, O_WRONLY | O_CREAT | O_TRUNC);
    for (int i = 0; i < 5000; i++) {
        io61_writec(w, (unsigned char)(i % 256));
    }
    io61_close(w);

    io61_file* f = io61_open_check(fname, O_RDONLY);
    std::vector<unsigned char> buf(300);

    io61_seek(f, 1000);
    io61_read(f, buf.data(), 200);

    io61_seek(f, 500);
    io61_read(f, buf.data(), 200);

    io61_seek(f, 3000);
    io61_read(f, buf.data(), 200);

    io61_seek(f, 0);
    io61_read(f, buf.data(), 200);

    io61_close(f);
    printf("zigzag seek test finished.\n");
}
