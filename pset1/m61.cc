#include "m61.hh"
#include <cstdlib>
#include <cstddef> 
#include <cstring>
#include <cstdio>
#include <cinttypes>
#include <cassert>
#include <sys/mman.h>
#include <map>


struct m61_memory_buffer {
    char* buffer;
    size_t pos = 0;
    size_t size = 8 << 20; /* 8 MiB */

    m61_memory_buffer();
    ~m61_memory_buffer();
};

static m61_memory_buffer default_buffer;


m61_memory_buffer::m61_memory_buffer() {
    void* buf = mmap(nullptr,    // Place the buffer at a random address
        this->size,              // Buffer should be 8 MiB big
        PROT_WRITE,              // We want to read and write the buffer
        MAP_ANON | MAP_PRIVATE, -1, 0);
                                 // We want memory freshly allocated by the OS
    assert(buf != MAP_FAILED);
    this->buffer = (char*) buf;
}

m61_memory_buffer::~m61_memory_buffer() {
    munmap(this->buffer, this->size);
}



/// m61_malloc(sz, file, line)
///    Returns a pointer to `sz` bytes of freshly-allocated dynamic memory.
///    The memory is not initialized. If `sz == 0`, then m61_malloc may
///    return either `nullptr` or a pointer to a unique allocation.
///    The allocation request was made at source code location `file`:`line`.

// Global statistics about memory allocations
static m61_statistics gstats = { 
    .nactive = 0, // Number of active allocations (not yet freed)
    .active_size = 0, // Total bytes in active allocations
    .ntotal = 0, // Total of allocation requests
    .total_size = 0, // Total bytes of all allocation requests
    .nfail = 0, // Number of failed allocation requests
    .fail_size = 0, // Total size of failed allocation requests
    .heap_min = 0, // Smallest allocated address
    .heap_max = 0 // Largest allocated address
};

// Struct to hold metadata for each allocation
struct allocationMetaData {
    size_t original_size; // Original size requested by the user
    size_t aligned_size;  // Size after alignment
    void* block_start; // Pointer to the start of the allocated block (including guard regions)
    const char* file; // File where allocation was made
    int line; // Line number where allocation was made
};

// Map to track active allocations and their sizes
static std::map<void*, allocationMetaData> active_allocations; 

// Map to track freed allocations and their sizes for reuse
static std::map<void*, allocationMetaData> freed_allocations; 

// Variable guard_size stores the size of guard regions using function std::max_align_t to ensure proper alignment
static const size_t guard_size = alignof(std::max_align_t); 

// Variable guard_pattern stores the pattern (0xEF) used to initialize guard regions around allocated memory
static const unsigned char guard_pattern = 0xEF; 

// Function to find a freed block of memory that can be reused for a new allocation request
static bool m61_find_free_space(size_t sz, void** reused_user_ptr, allocationMetaData* old_md){ 
   
    // Iterate through freed_allocations map to find a freed block that will fit
    for (auto it = freed_allocations.begin(); it != freed_allocations.end(); ++it) {
        
        // Variable that stores the metadata of the current block being looked at
        allocationMetaData m = it->second; 
        // Variable that stores the aligned size of the current block being looked at
        size_t available_bytes = m.aligned_size; 
        // Check if the reused freed block has enough space to satisfy the allocation request
        if (available_bytes >= sz){
            char* block_start = (char*) it->first; // Starting address of the current block being looked at
            char* user_ptr = block_start + guard_size; // Starting address of the user-accessible memory (taking into account guard size)
            
            // Check if the block can be reused as is (without splitting) -> there are 2 guard regions so multiply guard_size by 2
            if (available_bytes == sz || available_bytes < sz + 2 * guard_size + 1){
                memset(block_start, guard_pattern, guard_size); // Set block_start of reused block to guard pattern (front guard)
                memset(user_ptr + available_bytes, guard_pattern, guard_size); // Set user_ptr + sz of the reused block (end of block) to guard pattern (back guard)
                *reused_user_ptr = user_ptr; // Update the pointer at reused_user_ptr to point to the memory that now takes into account guard size
                *old_md = {0, sz, block_start, nullptr, 0}; // Update old_md to store the original and aligned sizes of the reused block
                freed_allocations.erase(it); // Remove the block from freed_allocations since it is being reused
                return true; // Return true becasue a suitable block was found and reused
            } 
        
            // Variable remainder_size stores the size of the remainder block if the reused block is split
            size_t remainder_size = available_bytes - sz - 2 * guard_size;

            // Split the reused block if it is large enough
            char* back_guard_reused = user_ptr + sz; // Pointer to the back guard of reused block
            char* remainder_start = back_guard_reused + guard_size; // Pointer to the start of the remainder block
            char* remainder_user_ptr = remainder_start + guard_size; // Pointer to the start of the reused memory in remainder block that is large enough for future allocations
            char* remainder_back_guard = remainder_user_ptr + remainder_size; // Pointer to the back guard of the remainder block
            
            memset(block_start, guard_pattern, guard_size); // Initalize front guard of reused block
            memset(back_guard_reused, guard_pattern, guard_size); // Initialize back guard of reused block
            memset(remainder_start, guard_pattern, guard_size); // Initialize front guard region of remainder block
            memset(remainder_back_guard, guard_pattern, guard_size); // Initialize back guard region of remainder block
            
            // Update the pointer by derefecning it and setting it to user_ptr (which takes into account guard size)
            *reused_user_ptr = user_ptr; 
            // Update the pointer at old_md to store the original size, aligned size, and block_start of the reused block
            *old_md = {0, sz, block_start, nullptr, 0}; 
            
            // Remove the old block from freed_allocations since it is being reused
            freed_allocations.erase(it); 
            // Add the remainder block to freed_allocations with original size 0 (since it is not being reused) and aligned size of remainder_size
            freed_allocations[remainder_start] = {0, remainder_size, remainder_start, nullptr, 0}; 
            return true; // Return true because a suitable block was found and reused
            }
        }
        return false; // Return false if no suitable block was found
    }   

// Malloc function with parameters for size, file, and line number (corresponds with error messgaes in m61_free)
void* m61_malloc(size_t sz, const char* file, int line) {  
    
    // Avoid uninitialized variable warnings
    (void) file, (void) line;
    
    // Check if no memory is being requested
    if (sz == 0) { 
        return nullptr; // Return nullptr if 0 requested
    }

    // Take the user requested size and store it in original_size for updating global statistics
    size_t original_size = sz; 

    // Overflow Check: If requested size is bigger than SIZE_MAX - 16 (to account for guard regions)
    if (sz > SIZE_MAX - 2 * guard_size){ 
        // Then update statistics for failed allocation
        ++gstats.nfail;
        gstats.fail_size += sz;
        return nullptr; // Return nullptr if overflow would occur
        }
    
    // Ensure 16-byte alignment
    size_t aligned_sz = sz; // Holds the size after alignment
    size_t remainder = aligned_sz % guard_size; // Holds the remainder when sz is divided by 16 
    
    if (remainder != 0) { // If remainder is not 0, then sz is not a multiple of 16
        size_t padding = guard_size - remainder; // Calculate how much padding is needed to make sz a multiple of 16 (and store in variable padding)
        if (aligned_sz > SIZE_MAX - padding) { // Check for overflow before adding padding
            // Update statistics and return nullptr if overflow occurs
            ++gstats.nfail; 
            gstats.fail_size += sz; 
            return nullptr; 
        }
        // Add padding to aligned_sz
        aligned_sz += padding; 
    }

    // Check if a freed block can be used first
    void* reused_ptr = nullptr; // Pointer to hold the address of the reused block
    allocationMetaData reused_md; // Variable to hold the metadata of the reused block
    
    // Implement m61_find_free_space to find a suitable freed block
    if (m61_find_free_space(aligned_sz, &reused_ptr, &reused_md)){
        char* userPointer = (char*)reused_ptr; // Pointer to the user-accessible memory of the reused block
        char* blockStart = (char*)reused_md.block_start; // Pointer to the start of the reused block that takes into account guard regions)
        
        // Initalize guard regions of the reused block
        memset(blockStart, guard_pattern, guard_size); 
        memset(userPointer + aligned_sz, guard_pattern, guard_size); 

        // Check if aligned_sz is greater than original_size
        if (aligned_sz > original_size){
            memset(userPointer + original_size, guard_pattern, aligned_sz - original_size); // Initialize the padding bytes to guard pattern
        }
    
         // Update statistics for successful allocation
         ++gstats.nactive; // Increase count of active allocations
         gstats.active_size += original_size; // Add sz to active allocation bytes 
         ++gstats.ntotal; // Increment total allocation count
         gstats.total_size += original_size; // Increment total allocation size by sz

         // Update metadata for this allocation in map active_allocations
         active_allocations[userPointer] = {original_size, aligned_sz, blockStart, file, line}; 
        
         // Update heap_min & heap_max
         uintptr_t allocBlock_Start = (uintptr_t) userPointer; // First address of the reused block 
         uintptr_t allocBlock_End = allocBlock_Start + aligned_sz - 1; // Last address of the reused block
         
         // Check if heap_min is 0 or if allocBlock_Start is smaller than heap_min
         if (gstats.heap_min == 0 || allocBlock_Start < gstats.heap_min) { // If new_FIRST is smaller than heap_min or if heap_min is 0
                gstats.heap_min = allocBlock_Start;// Update heap_min to allocBlock_Start
            }
        // Check if allocBlock_End is greater than heap_max
         if (allocBlock_End > gstats.heap_max) { 
            gstats.heap_max = allocBlock_End; // Update heap_max to new_last
            }
        return userPointer; // Return pointer to the user-accessible memory of the reused block
        }
    
    // Variable total_sz stores the total size needed for allocation including guard regions
    size_t total_sz = aligned_sz + 2 * guard_size; 
    
    // Begin allocation from default_buffer
    uintptr_t mem_address = (uintptr_t) &default_buffer.buffer[default_buffer.pos]; // Variable to hold current memory address in the buffer
    size_t alignRequirement = alignof(std::max_align_t); // Variable to hold the alignment requirement (16 bytes)
    uintptr_t alignedBlock_addr = (mem_address + (alignRequirement - 1)) & ~(uintptr_t)(alignRequirement - 1); // Variable to hold the aligned address of the block to be allocated
    size_t padding = (size_t)(alignedBlock_addr - mem_address); // Variable to hold the padding needed to align the address
    
    // Check if there is enough space in the buffer for the allocation (including padding and guard regions)
    if (padding + total_sz > default_buffer.size - default_buffer.pos) { 
        // Update statistics for failed allocation and return nullptr if there is not enoguh space
        ++gstats.nfail; 
        gstats.fail_size += original_size; 
        return nullptr;
    }

    char* blockStart = (char*) alignedBlock_addr; // Pointer to the start of the block to be allocated (including guard regions)
    char* user_ptr = blockStart + guard_size; // Pointer to the start of the user-acessible memory (after the front guard region)

    default_buffer.pos += padding + total_sz; 

    memset(blockStart, guard_pattern, guard_size); // Set front guard region to guard pattern
    memset(user_ptr + aligned_sz, guard_pattern, guard_size); // Set back guard region

    if (aligned_sz > original_size){
        memset(user_ptr + original_size, guard_pattern, aligned_sz - original_size); // Initialize the padding bytes to 0
    }

   
    // Update statistics because allocation was successful
    ++gstats.ntotal; 
    gstats.total_size += original_size; 
    ++gstats.nactive; 
    gstats.active_size += original_size; 
    // Update metadata for this allocation in map active_allocations
    active_allocations[user_ptr] = {original_size, aligned_sz, blockStart, file, line};

    // Update heap_min & heap_max
    uintptr_t new_first = (uintptr_t)user_ptr; // First address of the new allocation 
    uintptr_t new_last = new_first + aligned_sz - 1; // Last address of the new allocation

    // Check to be ensure heap_min and heap_max are updated correctly
    if (gstats.heap_min == 0 || new_first < gstats.heap_min) { 
            gstats.heap_min = new_first;
        }
    if (new_last > gstats.heap_max) { 
        gstats.heap_max = new_last; 
        }
    return user_ptr;
    }


/// m61_free(ptr, file, line)
///    Frees the memory allocation pointed to by `ptr`. If `ptr == nullptr`,
///    does nothing. Otherwise, `ptr` must point to a currently active
///    allocation returned by `m61_malloc`. The free was called at location
///    `file`:`line`. 

// Function that frees a previously allocated block of memory
void m61_free(void* user_pointer, const char* file, int line) {
    // Avoid uninitialized variable warnings
    (void) file, (void) line;

    // Handle nullptr case
    if (!user_pointer) {
        return;
    }

    // Check if pointer is in active_allocations
    auto active_alloc = active_allocations.find(user_pointer);
    if (active_alloc == active_allocations.end()) {
        
        // Check if pointer is in freed_allocations (first check for double free)
        void* possible_front = (void*)((char*)user_pointer - guard_size);
        auto freed_alloc = freed_allocations.find(possible_front);
        if (freed_alloc != freed_allocations.end()) {
            fprintf(stderr, "MEMORY BUG???: invalid free of pointer %p, double free\n", user_pointer);
            abort();
        }

        // Check to ensure pointer is within heap bounds
        uintptr_t addr = (uintptr_t) user_pointer; // Convert ptr to uintptr_t for comparison
        if (addr < gstats.heap_min || addr > gstats.heap_max) {
            fprintf(stderr, "MEMORY BUG???: invalid free of pointer %p, not in heap\n", user_pointer);
            abort(); // If not, return with error messae and then abort the program
        }

        // Check if pointer is within any active allocation (invalid free)
        for (auto& keyValue:active_allocations) {
            char* begin = (char*) keyValue.first;
            char* end = begin + keyValue.second.aligned_size;
            if ((char*) user_pointer > begin && (char*) user_pointer < end) {
                fprintf(stderr, "MEMORY BUG: %s:%d: invalid free of pointer %p, not allocated\n", file, line, user_pointer);
                abort(); 
            }
        }
    
        // Pointer is within heap bounds, so it must be a double free or invalid free
        fprintf(stderr, "MEMORY BUG???: invalid free of pointer %p, double free\n", user_pointer);
        abort();
        return;
    }

    // Retreive metadata for the active allocation
    allocationMetaData meta = active_alloc->second;

    // Create pointers to the start of the block, guard regions, and padding region
    char* blockStart = (char*)meta.block_start;
    char* guardStart = blockStart;
    char* guardEnd = (char*)user_pointer + meta.aligned_size;
    
    // Create pointers to the padding region (if needed)
    char* padStart = (char*)user_pointer + meta.original_size;
    char* padEnd = (char*)user_pointer + meta.aligned_size;
    
    // Check front guard region to ensure allocation can be freed
    for (size_t i = 0; i < guard_size; ++i) {
        if (((unsigned char*) guardStart)[i] != guard_pattern) {
            fprintf(stderr, "MEMORY BUG???: %s:%d: detected wild write during free of pointer %p\n", file, line, user_pointer);
            abort();
        }
    }

    // Check padding region to ensure allocation can be freed
    for (char* p = padStart; p < padEnd; ++p) {
        if (*(unsigned char*)p != guard_pattern) {
            fprintf(stderr, "MEMORY BUG???: %s:%d: detected wild write during free of pointer %p\n", file, line, user_pointer);
            abort();
        }
    }   

    // Check back guard region to ensure allocation can be freed
    for (size_t i = 0; i < guard_size; ++i) {
        if (((unsigned char*) guardEnd)[i] != guard_pattern) {
            fprintf(stderr, "MEMORY BUG???: %s:%d: detected wild write during free of pointer %p\n", file, line, user_pointer);
            abort();
        }
    }
    
    // Update statistics indicating one less active allocation
    gstats.nactive--;
    gstats.active_size -= meta.original_size;

    // Add block to freed_allocations for potential reuse
    allocationMetaData freed_metadata = {0, meta.aligned_size, meta.block_start, nullptr, 0}; // original_size is not needed for freed blocks, so set to 0
    auto freed_block = freed_allocations.insert({meta.block_start, freed_metadata}).first;


    // Coalesce with next block if adjacent
    while (true){
        auto next = std::next(freed_block);
        // Check if there is a next block
        if (next == freed_allocations.end()) {
            break; 
        }

        // Pointer to the end of the current block including guard regions
        char* end_of_current = (char*)freed_block->first + freed_block->second.aligned_size + 2 * guard_size;
        
        // Check if the next block is adjacent to the current block
        if (end_of_current != (char*)next->first) {
            break; 
        }

        // Merge the two blocks
        freed_block->second.aligned_size += next->second.aligned_size + 2 * guard_size;
        // Remove the next block from freed_allocations since it has been merged
        freed_allocations.erase(next);
    } 

    // Coalesce with previous block if adjacent
    while (freed_block != freed_allocations.begin()) {
        // Get the previous block
        auto prev = std::prev(freed_block);
        char* endPrev = (char*) prev->first + prev->second.aligned_size + 2 * guard_size;
        // Check if the previous block is adjacent to the current block
        if (endPrev != (char*) freed_block->first) {
            break; 
        }

        // Merge the two blocks
        prev->second.aligned_size += freed_block->second.aligned_size + 2 * guard_size;
        // Remove the current block from freed_allocations since it has been merged
        freed_allocations.erase(freed_block);
        // Update freed_block to point to the previous block for potential further coalescing
        freed_block = prev;
    }

    // Handle case where freed block is at the top of the buffer
    char* heapTop = default_buffer.buffer + default_buffer.pos; // Pointer to the top of the heap (end of used portion of buffer)
    char* freedBlock_end = (char*) freed_block->first + freed_block->second.aligned_size + 2 * guard_size; // Pointer to the end of the freed block including guard regions
    
    // Check if the freed block is at the top of the heap
    if (freedBlock_end == heapTop){
        default_buffer.pos = (size_t)((char*) freed_block->first - default_buffer.buffer); // Move the top of the heap down to the start of the freed block
        freed_allocations.erase(freed_block); // Erase the freed block from freed_allocations since it has been removed from the heap

        // While loop to coalesce any other adjacent freed blocks at the top of the heap
        while (!freed_allocations.empty()){ 
            // Find the last block in freed_allocations that is before the current top of the heap
            auto last = freed_allocations.upper_bound((void*)(default_buffer.buffer + default_buffer.pos));
            // Check if there is a last block
            if (last == freed_allocations.begin()) {
                    break;
                }
            // Get the previous block
            auto prev = std::prev(last);
            char* endBlock = (char*) prev->first + prev->second.aligned_size + 2 * guard_size;
            // Check if the previous block is adjacent to the current top of the heap
            if (endBlock != default_buffer.buffer + default_buffer.pos) {
                    break;
                }
            // Move the top of the heap down to the start of the previous block
            default_buffer.pos = (size_t)((char*) prev->first - default_buffer.buffer);
            // Remove the previous block from freed_allocations since it has been removed from the heap
            freed_allocations.erase(prev);
        }
        
    }
    // Erase the allocation from active_allocations since it has been freed
    active_allocations.erase(active_alloc);
    }


/// m61_calloc(count, sz, file, line)
///    Returns a pointer a fresh dynamic memory allocation big enough to
///    hold an array of `count` elements of `sz` bytes each. Returned
///    memory is initialized to zero. The allocation request was at
///    location `file`:`line`. Returns `nullptr` if out of memory; may
///    also return `nullptr` if `count == 0` or `size == 0`.

// Function that allocates memory for an array of x elements of y bytes each and ititializes all bytes to 0
void* m61_calloc(size_t count, size_t sz, const char* file, int line) {
    (void) file, (void) line; // Avoid unused variable warnings

    // Check if count or sz is 0
    if (count == 0 || sz == 0) { 
       return nullptr; 
    }
    
    // Check for overflow
    if (count > (SIZE_MAX / sz)) { 
        ++ gstats.nfail; 
        gstats.fail_size += count * sz; 
        return nullptr; 
    }
    
    // Variable total_size stores the total size needed for allocation
    size_t total_size = count * sz; 
    // Allocate memory using m61_malloc and store the returned pointer in ptr
    void* ptr = m61_malloc(total_size, file, line); 

    // Check if allocation failed
    if (!ptr) { 
        return nullptr;
    }
    
    // Set all bytes in the allocated memory to zero using memset
    memset(ptr, 0, total_size); 

    // Return pointer to the allocated memory
    return ptr;
}


/// m61_get_statistics()
///    Return the current memory statistics.

m61_statistics m61_get_statistics() {
    return gstats;
}


/// m61_print_statistics()
///    Prints the current memory statistics.

void m61_print_statistics() {
    m61_statistics stats = m61_get_statistics();
    printf("alloc count: active %10llu   total %10llu   fail %10llu\n",
           stats.nactive, stats.ntotal, stats.nfail);
    printf("alloc size:  active %10llu   total %10llu   fail %10llu\n",
           stats.active_size, stats.total_size, stats.fail_size);
      }


/// m61_print_leak_report()
///    Prints a report of all currently-active allocated blocks of dynamic
///    memory.

void m61_print_leak_report() {
    // Your code here.
}
