#include "m61.hh"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cinttypes>
#include <cassert>
#include <sys/mman.h>
#include <map>
#include <type_traits>
#include <new>
#include <cstddef>
#include <stddef.h>
#include <vector>

// each buffer is an 8 MiB chunk that m61_malloc can draw from
struct m61_memory_buffer {
    char* buffer;
    size_t pos = 0;
    size_t size = 8 << 20;   // 8 MiB

    m61_memory_buffer();
    ~m61_memory_buffer();
};

m61_memory_buffer::m61_memory_buffer() {
    void* buf = mmap(nullptr,          // random address
                     this->size,       // 8 MiB region
                     PROT_WRITE,       // readable + writable
                     MAP_ANON | MAP_PRIVATE,
                     -1, 0);
    assert(buf != MAP_FAILED);
    this->buffer = (char*) buf;
}

m61_memory_buffer::~m61_memory_buffer() {
    munmap(this->buffer, this->size);
}

// all heap space lives in these buffers (extra credit: multiple buffers)
static std::vector<m61_memory_buffer*> buffers;

// global statistics for the allocator
static m61_statistics gstats = {
    .nactive = 0,
    .active_size = 0,
    .ntotal = 0,
    .total_size = 0,
    .nfail = 0,
    .fail_size = 0,
    .heap_min = 0,
    .heap_max = 0
};

// per-allocation metadata
struct allocationMetaData {
    size_t original_size;       // bytes requested by user
    size_t aligned_size;        // size protected with guards
    void*  block_start;         // pointer to front guard
    const char* file;           // where the allocation was made
    int line;

    m61_memory_buffer* parent_buffer; // which buffer this lives in
    size_t total_span; //actual bytes consumed in buffer
};

// history of every allocation ever seen (for double-free / “inside region” errors)
static std::map<void*, allocationMetaData> allocation_history;

// currently-active allocations
static std::map<void*, allocationMetaData> active_allocations;

// per-buffer free lists: each buffer gets its own map of freed blocks
static std::map<m61_memory_buffer*, std::map<void*, allocationMetaData> > freed_lists;

// guards and padding use this size and pattern
// max_align_t is chosen so any type is safely aligned inside the block
static const size_t guard_size = alignof(std::max_align_t);
static const unsigned char guard_pattern = 0xEF;

// one-time initialization of the first buffer
static bool buffers_initialized = false;
static void ensure_initial_buffer() {
    if (!buffers_initialized) {
        buffers.push_back(new m61_memory_buffer());
        buffers_initialized = true;
    }
}

// helper: find which buffer contains a raw pointer (used as a fallback sanity check)
static m61_memory_buffer* find_buffer_for(void* p) {
    for (auto* buf : buffers) {
        char* start = buf->buffer;
        char* end   = buf->buffer + buf->size;
        if ((char*) p >= start && (char*) p < end) {
            return buf;
        }
    }
    return nullptr;
}

// find a freed block in `buf` that can satisfy an aligned request of size `sz`
// if found, returns true and fills `reused_user_ptr` and `old_md`
static bool m61_find_free_space(m61_memory_buffer* buf,
                                size_t sz,
                                void** reused_user_ptr,
                                allocationMetaData* old_md) {
    auto& freed_allocations = freed_lists[buf];

    for (auto it = freed_allocations.begin(); it != freed_allocations.end(); ++it) {
        allocationMetaData m = it->second;
        size_t available_bytes = m.aligned_size;

        if (available_bytes >= sz) {
            char* block_start = (char*) it->first;   // front guard
            char* user_ptr = block_start + guard_size;

            // case 1: reuse the whole block (no usable remainder)
            // if the leftover space would be too small to hold guards + payload,
            // just treat the whole range as the new block
            if (available_bytes == sz
                || available_bytes < sz + 2 * guard_size + 1) {

                // rewrite guards so this block behaves like a fresh allocation
                memset(block_start, guard_pattern, guard_size);
                memset(user_ptr + sz, guard_pattern, guard_size);

                *reused_user_ptr = user_ptr;

                // keep aligned_size = available_bytes,
                // since using the entire block as the active region
                *old_md = { 0, sz, block_start, nullptr, 0, buf, 0 };

                freed_allocations.erase(it);
                return true;
            }

            // case 2: split the block into [new block] + [remainder]
            size_t remainder_size = available_bytes - sz - 2 * guard_size;

            char* back_guard_reused   = user_ptr + sz;
            char* remainder_start     = back_guard_reused + guard_size;
            char* remainder_user_ptr  = remainder_start + guard_size;
            char* remainder_back_guard = remainder_user_ptr + remainder_size;

            // guards for reused portion
            memset(block_start, guard_pattern, guard_size);
            memset(back_guard_reused, guard_pattern, guard_size);

            // guards for remainder portion
            memset(remainder_start, guard_pattern, guard_size);
            memset(remainder_back_guard, guard_pattern, guard_size);

            *reused_user_ptr = user_ptr;
            *old_md = { 0, sz, block_start, nullptr, 0, buf, 0 };

            // remove original free block and insert remainder as a new free block
            freed_allocations.erase(it);
            freed_allocations[remainder_start] =
                { 0, remainder_size, remainder_start, nullptr, 0, buf, 0 };
            return true;
        }
    }

    return false;
}

/// m61_malloc(sz, file, line)
/// returns a pointer to `sz` bytes, surrounded by guard bytes.
/// may return nullptr on failure. does not call the system malloc.
void* m61_malloc(size_t sz, const char* file, int line) {
    
    ensure_initial_buffer();

    // malloc(0) is allowed. here i treat it as a “successful” allocation
    // that returns nullptr, but it still counts towards ntotal.
    if (sz == 0) {
        ++gstats.ntotal;
        // total_size += 0 is a no-op
        return nullptr;
    }

    size_t original_size = sz;

    // guard overflow: need space for sz + two guard regions
    if (sz > SIZE_MAX - 2 * guard_size) {
        ++gstats.nfail;
        gstats.fail_size += sz;
        return nullptr;
    }

    // round up to a multiple of guard_size so the payload is suitably aligned
    size_t aligned_sz = sz;
    size_t rem = aligned_sz % guard_size;
    if (rem != 0) {
        size_t padding = guard_size - rem;
        if (aligned_sz > SIZE_MAX - padding) {
            ++gstats.nfail;
            gstats.fail_size += sz;
            return nullptr;
        }
        aligned_sz += padding;
    }

    // if even one buffer cannot possibly hold this request, it is a hard fail
    if (aligned_sz + 2 * guard_size > buffers.back()->size) {
        ++gstats.nfail;
        gstats.fail_size += sz;
        return nullptr;
    }

    // first try to reuse a freed block from any existing buffer
    void* reused_ptr = nullptr;
    allocationMetaData reused_md;

    for (auto* b : buffers) {
        if (m61_find_free_space(b, aligned_sz, &reused_ptr, &reused_md)) {
            char* user_ptr   = (char*) reused_ptr;
            char* blockStart = (char*) reused_md.block_start;

            // guards were already written in m61_find_free_space
            // fill padding between original_size and aligned_sz with guard bytes
            if (aligned_sz > original_size) {
                memset(user_ptr + original_size,
                       guard_pattern,
                       aligned_sz - original_size);
            }

            // update stats
            ++gstats.nactive;
            gstats.active_size += original_size;
            ++gstats.ntotal;
            gstats.total_size += original_size;

            // forward dec 
            size_t total_span = aligned_sz + 2 * guard_size;

            // track this allocation in both active and history maps
            allocationMetaData md = {
                original_size,
                aligned_sz,
                blockStart,
                file,
                line,
                b, 
                total_span
            };
            active_allocations[user_ptr] = md;
            allocation_history[user_ptr] = md;

            uintptr_t start = (uintptr_t) user_ptr;
            uintptr_t end   = start + aligned_sz - 1;

            if (gstats.heap_min == 0 || start < gstats.heap_min) {
                gstats.heap_min = start;
            }
            if (end > gstats.heap_max) {
                gstats.heap_max = end;
            }

            return user_ptr;
        }
    }

    // no reusable block worked; allocate from the “top” of the latest buffer
    size_t total_sz = aligned_sz + 2 * guard_size;
    m61_memory_buffer* buf = buffers.back();

    // alignment explanation:

    // buf->buffer + buf->pos is the next free byte in this buffer.
    // want the user pointer to be aligned to alignof(max_align_t)
    // (typically 16 bytes on x86_64) so that any type can safely live there.
    //   do this by:
    //   1. interpreting the current address as an integer (uintptr_t),
    //   2. adding (align - 1), then
    //   3. clearing the low bits with a bitmask
    //
    // this rounds “mem_address” up to the next multiple of `align`.
    uintptr_t mem_address = (uintptr_t) (buf->buffer + buf->pos);
    size_t alignRequirement = alignof(std::max_align_t);
    uintptr_t alignedBlock_addr =
        (mem_address + (alignRequirement - 1)) & ~(uintptr_t) (alignRequirement - 1);
    size_t padding = (size_t) (alignedBlock_addr - mem_address);

    // if this buffer cannot fit the aligned block plus guards, get a new buffer
    if (padding + total_sz > buf->size - buf->pos) {
        buffers.push_back(new m61_memory_buffer());
        buf = buffers.back();

        mem_address = (uintptr_t) (buf->buffer + buf->pos);
        alignedBlock_addr =
            (mem_address + (alignRequirement - 1)) & ~(uintptr_t) (alignRequirement - 1);
        padding = (size_t) (alignedBlock_addr - mem_address);
    }

    // buf has space for the aligned block and guards
    char* blockStart = (char*) alignedBlock_addr;
    char* user_ptr   = blockStart + guard_size;

    size_t total_span = aligned_sz + 2 * guard_size;

    // advance the buffer “top” by the full span
    buf->pos += total_span;

    // write guard bytes around the payload
    memset(blockStart, guard_pattern, guard_size);
    memset(user_ptr + aligned_sz, guard_pattern, guard_size);

    // fill padding between “real” size and aligned size with guard bytes too
    if (aligned_sz > original_size) {
        memset(user_ptr + original_size,
               guard_pattern,
               aligned_sz - original_size);
    }

    // stats
    ++gstats.ntotal;
    gstats.total_size += original_size;
    ++gstats.nactive;
    gstats.active_size += original_size;

    allocationMetaData md = {
        original_size,
        aligned_sz,
        blockStart,
        file,
        line,
        buf, 
        total_span
    };
    active_allocations[user_ptr] = md;
    allocation_history[user_ptr] = md;

    uintptr_t start = (uintptr_t) user_ptr;
    uintptr_t end   = start + aligned_sz - 1;

    if (gstats.heap_min == 0 || start < gstats.heap_min) {
        gstats.heap_min = start;
    }
    if (end > gstats.heap_max) {
        gstats.heap_max = end;
    }

    return user_ptr;
}

/// m61_free(ptr, file, line)
/// frees an active allocation. detects invalid frees, double frees,
/// and simple wild writes using guard bytes.
void m61_free(void* user_pointer, const char* file, int line) {
    if (!user_pointer) {
        return;
    }

    auto active_alloc = active_allocations.find(user_pointer);

    // pointer is not currently active: figure out what kind of bug this is
    if (active_alloc == active_allocations.end()) {
        auto hist = allocation_history.upper_bound(user_pointer);
        if (hist != allocation_history.begin()) {
            --hist;

            char* base = (char*) hist->first;
            char* end  = base + hist->second.original_size;

            if (user_pointer == base) {
                fprintf(stderr,
                        "MEMORY BUG: %s:%d: invalid free of pointer %p, double free\n",
                        file, line, user_pointer);
                abort();
            }

            if ((char*) user_pointer > base && (char*) user_pointer < end) {
                fprintf(stderr,
                        "MEMORY BUG: %s:%d: invalid free of pointer %p, not allocated\n",
                        file, line, user_pointer);
                fprintf(stderr,
                        "  %s:%d: %p is %td bytes inside a %zu byte region allocated here\n",
                        hist->second.file,
                        hist->second.line,
                        user_pointer,
                        (char*) user_pointer - base,
                        hist->second.original_size);
                abort();
            }
        }

        uintptr_t addr = (uintptr_t) user_pointer;
        if (addr < gstats.heap_min || addr > gstats.heap_max) {
            fprintf(stderr,
                    "MEMORY BUG???: invalid free of pointer %p, not in heap\n",
                    user_pointer);
            abort();
        }

        fprintf(stderr,
                "MEMORY BUG: %s:%d: invalid free of pointer %p, not allocated\n",
                file, line, user_pointer);
        abort();
    }

    // now know its an active alloc
    allocationMetaData meta = active_alloc->second;

    // use parent_buffer directly; fall back to address-based search if needed
    m61_memory_buffer* buf = meta.parent_buffer;
    if (!buf) {
        buf = find_buffer_for(meta.block_start);
    }
    if (!buf) {
        fprintf(stderr,
                "MEMORY BUG: %s:%d: invalid free of pointer %p, not in any buffer\n",
                file, line, user_pointer);
        abort();
    }

    char* blockStart = (char*) meta.block_start;
    char* guardStart = blockStart;
    char* guardEnd   = (char*) user_pointer + meta.aligned_size;

    char* padStart = (char*) user_pointer + meta.original_size;
    char* padEnd   = (char*) user_pointer + meta.aligned_size;

    // front guard check
    for (size_t i = 0; i < guard_size; ++i) {
        if (((unsigned char*) guardStart)[i] != guard_pattern) {
            fprintf(stderr,
                    "MEMORY BUG???: %s:%d: detected wild write during free of pointer %p\n",
                    file, line, user_pointer);
            abort();
        }
    }

    // padding check (between user size and aligned size)
    for (char* p = padStart; p < padEnd; ++p) {
        if (*(unsigned char*) p != guard_pattern) {
            fprintf(stderr,
                    "MEMORY BUG???: %s:%d: detected wild write during free of pointer %p\n",
                    file, line, user_pointer);
            abort();
        }
    }

    // back guard check
    for (size_t i = 0; i < guard_size; ++i) {
        if (((unsigned char*) guardEnd)[i] != guard_pattern) {
            fprintf(stderr,
                    "MEMORY BUG???: %s:%d: detected wild write during free of pointer %p\n",
                    file, line, user_pointer);
            abort();
        }
    }

    // update stats for one fewer active allocation
    gstats.nactive--;
    gstats.active_size -= meta.original_size;

    // insert this block into the per-buffer free list
    auto& freed_allocations = freed_lists[buf];
    allocationMetaData freed_metadata =
        { 0, meta.aligned_size, meta.block_start, nullptr, 0, buf, 0 };
    auto freed_block = freed_allocations.insert({ meta.block_start, freed_metadata }).first;

    // coalesce with next block if it is immediately after us
    while (true) {
        auto next = std::next(freed_block);
        if (next == freed_allocations.end()) {
            break;
        }

        char* end_of_current =
            (char*) freed_block->first
            + freed_block->second.aligned_size
            + 2 * guard_size;

        if (end_of_current != (char*) next->first) {
            break;
        }

        freed_block->second.aligned_size +=
            next->second.aligned_size + 2 * guard_size;
        freed_allocations.erase(next);
    }

    // coalesce with previous block if it is immediately before us
    while (freed_block != freed_allocations.begin()) {
        auto prev = std::prev(freed_block);
        char* endPrev =
            (char*) prev->first
            + prev->second.aligned_size
            + 2 * guard_size;

        if (endPrev != (char*) freed_block->first) {
            break;
        }

        prev->second.aligned_size +=
            freed_block->second.aligned_size + 2 * guard_size;
        freed_allocations.erase(freed_block);
        freed_block = prev;
    }

    // if the top of the heap is now a free block, move buf->pos down
    char* heapTop = buf->buffer + buf->pos;
    char* freedBlock_end =
        (char*) freed_block->first
        + freed_block->second.aligned_size
        + 2 * guard_size;

    if (freedBlock_end == heapTop) {
        buf->pos = (size_t) ((char*) freed_block->first - buf->buffer);
        freed_allocations.erase(freed_block);

        // keep pulling the top down while the last bytes belong to freed blocks
        while (!freed_allocations.empty()) {
            auto last = freed_allocations.upper_bound((void*) (buf->buffer + buf->pos));
            if (last == freed_allocations.begin()) {
                break;
            }

            auto prev = std::prev(last);
            char* endBlock =
                (char*) prev->first
                + prev->second.aligned_size
                + 2 * guard_size;

            if (endBlock != buf->buffer + buf->pos) {
                break;
            }

            buf->pos = (size_t) ((char*) prev->first - buf->buffer);
            freed_allocations.erase(prev);
        }
    }

    active_allocations.erase(active_alloc);
}

/// m61_calloc(count, sz, file, line)
/// allocate count * sz bytes and zero them.
void* m61_calloc(size_t count, size_t sz, const char* file, int line) {
    if (count == 0 || sz == 0) {
        return nullptr;
    }

    if (count > SIZE_MAX / sz) {
        ++gstats.nfail;
        gstats.fail_size += count * sz;
        return nullptr;
    }

    size_t total_size = count * sz;
    void* ptr = m61_malloc(total_size, file, line);
    if (!ptr) {
        return nullptr;
    }

    memset(ptr, 0, total_size);
    return ptr;
}

m61_statistics m61_get_statistics() {
    return gstats;
}

/// m61_realloc(ptr, sz, file, line)
/// simple realloc built on top of m61_malloc and m61_free.
void* m61_realloc(void* ptr, size_t sz, const char* file, int line) {
    if (!ptr) {
        return m61_malloc(sz, file, line);
    }

    if (sz == 0) {
        // spec says sz must not be 0, so treat as malloc(1)
        return m61_malloc(1, file, line);
    }

    auto it = active_allocations.find(ptr);
    if (it == active_allocations.end()) {
        // cannot safely realloc something not currently allocated
        return nullptr;
    }

    allocationMetaData old = it->second;
    size_t old_size = old.original_size;

    // shrinking in place is easy
    if (sz <= old_size) {
        gstats.active_size -= (old_size - sz);
        it->second.original_size = sz;
        return ptr;
    }

    // need a bigger block
    void* new_ptr = m61_malloc(sz, file, line);
    if (!new_ptr) {
        return nullptr;
    }

    memcpy(new_ptr, ptr, old_size);
    m61_free(ptr, file, line);
    return new_ptr;
}

void m61_print_statistics() {
    m61_statistics stats = m61_get_statistics();
    printf("alloc count: active %10llu   total %10llu   fail %10llu\n",
           stats.nactive, stats.ntotal, stats.nfail);
    printf("alloc size:  active %10llu   total %10llu   fail %10llu\n",
           stats.active_size, stats.total_size, stats.fail_size);
}

void m61_print_leak_report() {
    for (auto& it : active_allocations) {
        void* user_ptr = it.first;
        allocationMetaData md = it.second;
        printf("LEAK CHECK: %s:%d: allocated object %p with size %zu\n",
               md.file, md.line, user_ptr, md.original_size);
    }
}
