#include "m61.hh"
#include <cstdio>
// Check that never-allocated memory can be coalesced with freed memory.

int main() {
  void *p = m61_malloc(1 << 22); // ~half the buffer
  assert(p);
  m61_free(p);

  p = m61_malloc((1 << 22) + (1 << 21)); // ~75% of the buffer
  assert(p);

  m61_statistics stats = m61_get_statistics();
  assert((uintptr_t) p >= stats.heap_min);
  assert((uintptr_t) p + (1 << 22) + (1 << 21) - 1 <= stats.heap_max);
  m61_print_statistics();
}

//! alloc count: active          1   total          2   fail          0
//! alloc size:  active    6291456   total   10485760   fail          0
