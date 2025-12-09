#include "m61.hh"
#include <cstdio>
// Ensure an obvious double-free is be caught even if coalesced.

int main() {
  void *p = m61_malloc(32);
  assert(p);
  void *q = m61_malloc(48);
  assert(q);
  fprintf(stderr, "Will double free %p\n", q);
  m61_free(p);
  m61_free(q);
  m61_free(q);
}


//! Will double free ??{0x\w+}=ptr??
//! MEMORY BUG???: invalid free of pointer ??ptr??, double free
//! ???
//!!ABORT
