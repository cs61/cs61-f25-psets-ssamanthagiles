#include <cstdio>

int main() {
    unsigned long n = 0;
    while (fgetc(stdin) != EOF) {
        n++;
    }
    printf("%lu\n", n);
}