#include "m61.hh"
#include <cstdio>
#include <cassert>
#include <cstring>

int main() {
    // Allocate 20 bytes using realloc with nullptr (should behave like malloc)
    char* p = (char*) realloc(nullptr, 20); 
    // Print the pointer returned by realloc  
    // Shows hex address of allocated memory
    printf("ptr = %p\n", p);
    // Use free to deallocate the memory
    free(p);      
}

