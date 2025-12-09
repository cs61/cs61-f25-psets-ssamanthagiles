#include <cstdio>
#include <cctype>

int main() {
    // Initialize counters for characters, words, and lines
    unsigned long nc = 0, nw = 0, nl = 0;
    // A flag to track if we are currently in a sequence of spaces
    bool in_spaces = true;
    // Read characters until EOF
    while (true) {
        // Read a character from standard input
        int ch = fgetc(stdin);
        if (ch == EOF) {
            break;
        }
        nc++;
        // Check if ch is a space, tab, newline, or other whitespace, and store the result in this_space.”
       bool this_space = isspace((unsigned char)ch);
       if (this_space && !in_spaces) {
              nw++;
        }
    // Update in_spaces to reflect whether the current character is a space
    in_spaces = this_space;
    // If the character is a newline, increment the line counter
    if (ch == '\n') {
        nl++;
    }
    // After the loop, if we were in a word when EOF was reached, count that word
    fprintf(stdout, "%8lu %7lu %7lu\n", nl, nw, nc);
    }
}