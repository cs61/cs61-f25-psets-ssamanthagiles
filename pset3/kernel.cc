#include "kernel.hh"
#include "k-apic.hh"
#include "k-vmiter.hh"
#include "obj/k-firstprocess.h"
#include <atomic>

// kernel.cc
//
// This is the kernel.


// INITIAL PHYSICAL MEMORY LAYOUT
//
//  +-------------- Base Memory --------------+
//  v                                         v
// +-----+--------------------+----------------+--------------------+---------/
// |     | Kernel      Kernel |       :    I/O | App 1        App 1 | App 2
// |     | Code + Data  Stack |  ...  : Memory | Code + Data  Stack | Code ...
// +-----+--------------------+----------------+--------------------+---------/
// 0  0x40000              0x80000 0xA0000 0x100000             0x140000
//                                             ^
//                                             | \___ PROC_SIZE ___/
//                                      PROC_START_ADDR

#define PROC_SIZE 0x40000       // initial state only

proc ptable[MAXNPROC];          // array of process descriptors
                                // Note that `ptable[0]` is never used.
proc* current;                  // pointer to currently executing proc

#define HZ 100                  // timer interrupt frequency (interrupts/sec)
[[maybe_unused]] static std::atomic<unsigned long> ticks; // # timer interrupts so far

// Memory state - see `kernel.hh`
physpageinfo physpages[NPAGES];


[[noreturn]] void schedule();
[[noreturn]] void run(proc* p);
void exception(regstate* regs);
uintptr_t syscall(regstate* regs);
void memshow();

// declare syscall_exit for use later
static void syscall_exit(int status);

// helper functions below to initialize a process structure
// setup a process's address space and load its program
static void process_setup(pid_t pid, const char* program_name);
// page size is 4096 bytes
// round x up to nearest page boundary
static inline void incref_pa(uintptr_t pa) {
    int pn = pa / PAGESIZE;
    assert(pn >= 0 && pn < NPAGES);
    assert(physpages[pn].refcount >= 0);
    ++physpages[pn].refcount;
}

// decrement refcount for a physical page
static inline void decref_pa(uintptr_t pa) {
    int pn = pa / PAGESIZE;
    assert(pn >= 0 && pn < NPAGES);
    assert(physpages[pn].refcount > 0);
    --physpages[pn].refcount;
}

// memory cleanup to unmap a virtual address and maybe free the associated physical page
static inline void unmap_and_maybe_free(x86_64_pagetable* pt, uintptr_t va) {
    
    // iterate through page table to find mapping for va
    vmiter it(pt, va);
    // if not mapped, don't proceed with unmapping
    if (!it.present()) {
        return;
    }
    // get physical address before unmapping
    uintptr_t pa = it.pa();

    // unmap the virtual address
    int r = it.try_map((uintptr_t) 0, 0);
    assert(r == 0);

    // check if physical address is valid
    if (pa == 0) {
        return;
    }
    // only decrement refcount if physical address is in user space
    if (va >= PROC_START_ADDR && va < MEMSIZE_VIRTUAL) {
        decref_pa(pa);
    }
}

// process cleanup to free all memory associated with a process's address space
static void free_process_address_space(proc* p) {
    // get the process's page table
    x86_64_pagetable* pt = p->pagetable;

    // if no page table, nothing to free
    if (!pt){
        return;
    }
    
    // unmap all user-space pages
    for (vmiter it(pt, PROC_START_ADDR); it.va() < MEMSIZE_VIRTUAL; it.next()) {
        if (it.present()) {
            unmap_and_maybe_free(pt, it.va());
        }
    }

    // using ptiter to iterate over all page table entries
    for (ptiter it(pt); it.va() < MEMSIZE_VIRTUAL; it.next()) {
        uintptr_t ppa = it.pa();
        // free lower level page table pages
        if (ppa) {
            kfree(pa2kptr(ppa));
        }
    }

    // free top-level page table page
    kfree(pt);

    // set process's pagetable to nullptr
    p->pagetable = nullptr;
}


// entry point for WeensyOS kernel, using command to hold the name of the first user process
void kernel_start(const char* command) {

    // initialize hardware
    init_hardware();
    log_printf("Starting WeensyOS\n");

    // initialize physical memory state
    ticks = 1; 
    init_timer(HZ);

    // clear screen
    console_clear();

    // assert that console address is page-aligned
    static_assert((CONSOLE_ADDR % PAGESIZE) == 0,
                  "Console address must be page-aligned");

    // walk through all physical pages 
    for (uintptr_t pa = PAGESIZE; pa < MEMSIZE_PHYSICAL; pa += PAGESIZE) {
        // mark kernel pages as in-use, so they are not allocated by user processes
        int r = vmiter(kernel_pagetable, pa).try_map(pa, PTE_P | PTE_W); 
        assert(r == 0);
    }

    // set up process scheduling table
    for (pid_t i = 0; i < MAXNPROC; i++) {
        // set process ID (pid)
        ptable[i].pid = i;
        // mark all processes as free
        ptable[i].state = P_FREE;
    }

    // if no command is provieded by user, use default first process provided by WEENSYOS_FIRST_PROCESS 
    if (!command) {
        command = WEENSYOS_FIRST_PROCESS;
    }

    //  if user specified a program, set it up as first process
    if (!program_image(command).empty()) {
        process_setup(1, command);

    // else, set up default allocator processes
    } else {
        process_setup(1, "allocator");
        process_setup(2, "allocator2");
        process_setup(3, "allocator3");
        process_setup(4, "allocator4");
    }

    // switch to first process using run()
    run(&ptable[1]);
}

// reallocate physical memory page after checking its validity
void* kalloc(size_t sz) {

    // only support allocation of single page
    if (sz > PAGESIZE) {
        return nullptr;
    }

    // variables to track page allocation
    static int cursor = 1; 
    static int page_increment = 3; // phase 4
    const int total_pages = NPAGES - 1;

    // try to find a free page
    for (int tries = 0; tries < total_pages; ++tries) {
        // when reaching the end (total_pages), wrap around to beginning (cursor)
        int pageno = (cursor + tries * page_increment) % NPAGES;
        // skip page 0 bc its reserved to catch null pointer derefs (seg faults)
        if (pageno == 0) {
            pageno = 1;
        }

        // calculate physical address of the page
        uintptr_t pa = (uintptr_t) pageno * PAGESIZE;

        // only consider allocatable physical addresses inside RAM
        // can't be physical addresses used for kernel or memory-mapped I/O
        if (!allocatable_physical_address(pa) || pa >= MEMSIZE_PHYSICAL) {
            continue;
        }

        // if a physical page has a refcount of 0, it is free to allocate
        if (physpages[pageno].refcount == 0) {
            // now, mark the page as in-use
            ++physpages[pageno].refcount;
            // convert physical address to kernel pointer
            void* kptr = pa2kptr(pa);
            // set all bytes in the page to zero
            memset(kptr, 0, PAGESIZE);
            // update cursor for next allocation
            cursor = (pageno + page_increment) % NPAGES;
            // if cursor = 0 (after wrap-around), set to 1
            if (cursor == 0) {
                cursor = 1;
            }
            // return pointer to allocated page
            return kptr;
        }
    }

    // can't find any free pages, return nullptr
    return nullptr; 
}

// free physical memory page after checking its validity
void kfree(void* kptr) {

    // if kptr is null, do nothing
    if (!kptr) {
        return;
    }

    // take address of kptr and calculate page number
    uintptr_t pa = reinterpret_cast<uintptr_t>(kptr);
    // align pa down to page boundary
    int pageno = pa / PAGESIZE;

    // check if kptr is a valid allocatable physical address
    assert(allocatable_physical_address(pa)); 
    // prevent double free by ensuring page is actually in use
    assert(physpages[pageno].refcount > 0);  

    // now mark the page as free
    --physpages[pageno].refcount; 

    // log check, should be 0 after free
    log_printf("Freed page: pa=%p, refcount=%d\n", pa, physpages[pageno].refcount);
}

// sets up a user process's address space and loads its program
void process_setup(pid_t pid, const char* program_name) {
    // initialize process descriptor, pointer to kernel page table
    init_process(&ptable[pid], 0);
 
 
    // allocate a new page table for the process
    x86_64_pagetable* pt = kalloc_pagetable();
    
    // make sure new page table allocation succeeded
    assert(pt != nullptr);
    
    // copy kernel mappings into process page table
    for (vmiter kit(kernel_pagetable, 0); kit.va() < PROC_START_ADDR; kit.next()) {
        // skip unmapped entries
        if (!kit.present()) {
            continue;
        }
        
        // use kit.perm to retrieve current permissions
        // remove user accessble permissions from the page table entry so that
        // user processes cannot access kernel memory
        int perms = kit.perm() & ~PTE_U;  

        // if mapping is for console memory-mapped I/O region, make it user-accessible & writable
        if (kit.va() >= CONSOLE_ADDR && kit.va() < CONSOLE_ADDR + PAGESIZE) {
                perms |= PTE_U | PTE_W; 
        }

        // now map page into process page table through vmiter 
        int r = vmiter(pt, kit.va()).try_map(kit.pa(), perms);
        // check if mapping succeeded
        assert(r == 0);
    }

    // initialize process page table with newly mapped kernel memory
    ptable[pid].pagetable = pt;

    // obtain reference to program image
    // (The program image models the process executable)
    program_image pgm(program_name);

    // phase 5, shared virtual address space for all processes
    const uintptr_t user_lo = PROC_START_ADDR;
    const uintptr_t user_hi = MEMSIZE_VIRTUAL;

 
    // map & load each loadable user program segment in the program image
    for (auto seg = pgm.begin(); seg != pgm.end(); ++seg) {
        
        // skip empty segments
        if (seg.size() == 0) {
            continue;
        }
        
        // variables to hold user segment properties
        const bool writable = seg.writable();
        const uintptr_t seg_start = seg.va();
        const uintptr_t seg_end = seg_start + seg.size();

        // check if segment is within correct user address space
        assert(seg_start >= user_lo && seg_end <= user_hi);

        // logging segment mapping
        log_printf("SEG map: start=%p end=%p (size=%#zx)\n",
            (void*)seg_start, (void*)seg_end, (size_t)seg.size());

        // map every virtual address in segment to newly allocated physical page
        for (uintptr_t va = seg_start & ~(PAGESIZE - 1);
        va < seg_end;
        va += PAGESIZE) {
            // allocate a physical page
            void* kptr = kalloc(PAGESIZE);
            // check if allocation worked
            if (!kptr) {
                panic("Out of memory while mapping program segment");
            }
            // get physical address of allocated page
            uintptr_t pa = kptr2pa(kptr);

            // iterate through page table to map va to pa with correct permissions
            int r = vmiter(pt, va).try_map(
                pa, PTE_P | PTE_U | (writable ? PTE_W : 0)); // writable flag
            // check if mapping succeeded
            assert(r == 0);
        }
    
        // copy initalized bytes from virtual address page by page through to physical addresses
        size_t n = seg.data_size();
        size_t off = 0;
        // while there is still data to copy
        while (off < n) {
            // calculate user virtual address within segment
            uintptr_t a = seg_start + off;
            // use vmiter to find physical address mapped to user virtual address 
            vmiter it(pt, a); 
            // get physical address
            uintptr_t pa = it.pa();
            // check if physical address is valid
            assert(pa != 0);

            // copy one page worth of data
            size_t page_off = a & (PAGESIZE - 1);
            // calculate how much room is left on page
            size_t room = PAGESIZE - page_off;
            // calculate how much data is left to copy
            size_t chunk = (n - off < room) ? (n - off) : room;
    
            // perform the copy from segment to physical memory
            memcpy((char*) pa2kptr(pa) + page_off, seg.data() + off, chunk);
            // and advance offset to be copied in segment if needed
            off += chunk;
        }
    }   

    // variables to hold stack properties
    const uintptr_t stack_top  = MEMSIZE_VIRTUAL;
    const uintptr_t stack_page = stack_top - PAGESIZE;

    // allocate physical page through kalloc for user stack
    void* sp = kalloc(PAGESIZE);
    // check if allocation worked
    assert(sp);
    // get physical address of stack page
    uintptr_t spa = kptr2pa(sp);

    // iterate through page table to map user stack page
    int r = vmiter(pt, stack_page).try_map(spa, PTE_P | PTE_U | PTE_W);
    // check if mapping worked
    assert(r == 0);
    // set all bytes in stack page to zero after mapping
    memset(pa2kptr(spa), 0, PAGESIZE);

    // initialize user registers
    ptable[pid].regs.reg_rsp = stack_top;
    ptable[pid].regs.reg_rip = pgm.entry();

    // log process stack mapping
    log_printf("STACK: pid=%d stack_page=%p spa=%p\n",
        pid, (void*)stack_page, (void*)spa);

    // stack mapping complete, set process state to runnable
    ptable[pid].state = P_RUNNABLE;

}

 // exceptions for the kernel
 void exception(regstate* regs) {
    // Copy the saved registers into the `current` process descriptor.
    current->regs = *regs;
    regs = &current->regs;
 
    // Show the current cursor location and memory state
    // (unless this is a kernel fault).
    console_show_cursor();
    if (regs->reg_intno != INT_PF || (regs->reg_errcode & PTE_U)) {
        memshow();
    }
 
    // If Control-C was typed, exit the virtual machine.
    check_keyboard();
 
    // Actually handle the exception.
    switch (regs->reg_intno) {
 
    case INT_IRQ + IRQ_TIMER:
        ++ticks;
        lapicstate::get().ack();
        schedule();
        break;                  /* will not be reached */
 
 
    case INT_PF: {
        // Analyze faulting address and access type.
        uintptr_t addr = rdcr2();
        const char* operation = regs->reg_errcode & PTE_W
                ? "write" : "read";
        const char* problem = regs->reg_errcode & PTE_P
                ? "protection problem" : "missing page";
 
 
        if (!(regs->reg_errcode & PTE_U)) {
            proc_panic(current, "Kernel page fault on %p (%s %s, rip=%p)!\n",
                       addr, operation, problem, regs->reg_rip);
        }
        error_printf("PAGE FAULT on %p (pid %d, %s %s, rip=%p)!\n",
                     addr, current->pid, operation, problem, regs->reg_rip);
        log_print_backtrace(current);
        current->state = P_FAULTED;
        break;
    }
 
 
    default:
        proc_panic(current, "Unhandled exception %d (rip=%p)!\n",
                   regs->reg_intno, regs->reg_rip);
 
 
    }
 
    // Return to the current process (or run something else).
    if (current->state == P_RUNNABLE) {
        run(current);
    } else {
        schedule();
    }
 }
 
 // declaring function syscall_page_alloc for use later
 int syscall_page_alloc(uintptr_t addr);

 static void assert_no_shared_writable(x86_64_pagetable* pt) {
    // iterate through all user va mappigns in the page table
    for (vmiter it(pt, PROC_START_ADDR); it.va() < MEMSIZE_VIRTUAL; it.next()) {
        // skip unmapped entries
        if (!it.present()) {
            continue;
        }
        // if mapping is writable, ensure its refcount is 1 (no sharing)
        if (it.writable()) {
            // get physical page number of virtual address mapping
            int pn = it.pa() / PAGESIZE;
            assert(physpages[pn].refcount == 1);
        }
    }
}

// create a new process by duplicating the current process
static int syscall_fork() {
    // cpid = child process id
    pid_t cpid = -1;

    // find free slot in process table for child
    for (pid_t i = 1; i < MAXNPROC; ++i) {
        if (ptable[i].state == P_FREE) { cpid = i; break; }
    }

    // check if search was successful
    if (cpid < 0) {
        return -1; // no slot
    }
    
    // make sure cpid slot is marked free
    init_process(&ptable[cpid], 0);
    
    // pointer to child page table
    x86_64_pagetable* cpt = nullptr;
    
    // allocate child page table through kalloc_pagetable
    cpt = kalloc_pagetable();
    // check if child page table allocation succeeded
    if (!cpt) {
        // allocation failed, mark cpid slot as free again
        ptable[cpid].state = P_FREE;
        return -1;
    }
    
    // map a va/pa pair into child page table with given permissions
    auto map_child = [&](uintptr_t va, uintptr_t pa, int perm) -> int {
        return vmiter(cpt, va).try_map(pa, perm);
    };
    
    // used during fork to copy console mapping exactly as it exists in parent process
    for (vmiter pit(current->pagetable, 0); pit.va() < PROC_START_ADDR; pit.next()) {
        // check if there's a valid physical address to map
        if (!pit.present()) {
            continue;
        }
        // get permissions for mapping
        int perms = pit.perm();
        // make console mapping user-accessible & writable in child
        if (pit.va() >= CONSOLE_ADDR && pit.va() < CONSOLE_ADDR + PAGESIZE) {
            perms |= PTE_U | PTE_W; // console is user-accessible & writable
        } else {
            perms &= ~PTE_U; // other kernel mappings not user-accessible
        }
        // map va/pa pair into child page table with permissions 
        if (map_child(pit.va(), pit.pa(), perms) < 0) goto fail;
    }
    
    // used during process_setup to adjust permissions of console mapping to be PTE_U | PTE_W
    for (vmiter pit(current->pagetable, PROC_START_ADDR);
        pit.va() < MEMSIZE_VIRTUAL; pit.next()) {
        
        // check if there's a valid physical address to map
        if (!pit.present()) {
            continue;
        }

        // get virtual & physical addresses for mapping
        uintptr_t va  = pit.va();
        uintptr_t ppa = pit.pa();

        // get the console mapping exactly as-is (no incrementing refcount)
        // bc have not allocated a new physical page for it
        if (va >= CONSOLE_ADDR && va < CONSOLE_ADDR + PAGESIZE) {
            if (vmiter(cpt, va).try_map(ppa, pit.perm()) < 0) goto fail;
            continue;
        }

        // now handle regular user page mappings
        if (pit.writable()) {
            // use kalloc to allocate new physical page for child
            void* newk = kalloc(PAGESIZE);
            // if allocation fails, goto fail
            if (!newk) goto fail;
            // copy contents from parent's physical page to child's new physical page
            uintptr_t cpa = kptr2pa(newk);
            // perform the copy
            memcpy(pa2kptr(cpa), pa2kptr(ppa), PAGESIZE);
            // map new physical page into child page table with read & user acess permissions
            if (vmiter(cpt, va).try_map(cpa, (pit.perm() | PTE_P | PTE_U)) < 0) goto fail;
            } else {
                // increment refcount for shared read-only page
                incref_pa(ppa);
                // can't be writable in child if not writable in parent
                // so remove PTE_W from permissions
                int ro_perms = pit.perm() & ~PTE_W;
                // check if mapping into child page table works
                if (vmiter(cpt, va).try_map(ppa, ro_perms) < 0) goto fail;
            }
    }
    // initialize child process descriptor
    ptable[cpid].pagetable = cpt;
    ptable[cpid].regs = current->regs;
    // set child's return value from fork to differntiate from parent's return value
    ptable[cpid].regs.reg_rax = 0; 
    ptable[cpid].state = P_RUNNABLE;
    
    // runtime guard 
    assert_no_shared_writable(current->pagetable);
    assert_no_shared_writable(cpt);
    
    // return child process id to parent
    return cpid;
    
    fail:

    // clean any partial child mappings & page tables to prevent leaks
    if (cpt) {
        // unmap any user mappings already created in the child
        for (vmiter it(cpt, PROC_START_ADDR); it.va() < MEMSIZE_VIRTUAL; it.next()) {
            if (it.present()) {
                unmap_and_maybe_free(cpt, it.va());
            }
        }

        // free page-table pages
        for (ptiter it(cpt); it.va() < MEMSIZE_VIRTUAL; it.next()) {
            uintptr_t ppa = it.pa();
                if (ppa) {
                    kfree(pa2kptr(ppa));
                }
            }
            // free top-level pt
            kfree(cpt);
        }
        // free cpid slot
        ptable[cpid].state = P_FREE;
        // avoid invalid pointer
        ptable[cpid].pagetable = nullptr;
        // indicate failure
        return -1;
    }
    
// handle system calls from user processes
 uintptr_t syscall(regstate* regs) {
    // copy the saved registers into the current process descriptor.
    current->regs = *regs;
    regs = &current->regs;

    // Show the current cursor location and memory state.
    console_show_cursor();
    memshow();
 
 
    // If Control-C was typed, exit the virtual machine.
    check_keyboard();
 
 
 
 
    // Actually handle the exception.
    switch (regs->reg_rax) {
 
 
    case SYSCALL_PANIC:
        user_panic(current);
        break; // will not be reached
 
 
    case SYSCALL_GETPID:
        return current->pid;
 
 
    case SYSCALL_YIELD:
        current->regs.reg_rax = 0;
        schedule();             // does not return
 
 
    case SYSCALL_PAGE_ALLOC:
        return syscall_page_alloc(current->regs.reg_rdi);


    case SYSCALL_EXIT:
        syscall_exit((int) current->regs.reg_rdi);   // does not return
        break; // not reached
    
    // phase 6
    case SYSCALL_FORK:
        return syscall_fork();

    
 
    default:
        proc_panic(current, "Unhandled system call %ld (pid=%d, rip=%p)!\n",
                   regs->reg_rax, current->pid, regs->reg_rip);
 
    }
 
 
    panic("Should not get here!\n");
 }
 
 
 // phase 7, terminate current process and free its resources
 static void syscall_exit(int status) {
    // avoid unused parameter warning
    (void) status; 
    // get pointer to current process
    proc* p = current;

    // free entire address space (ser mappings + page-table pages)
    free_process_address_space(p);

    // mark slot free and switch away forever
    p->state = P_FREE;
    // use schedule function to switch to another process
    schedule(); 
    __builtin_unreachable();
}

 // allocate a new page at the given virtual address in the current process
 int syscall_page_alloc(uintptr_t addr) {
        // must be page-aligned
        if ((addr & (PAGESIZE - 1)) != 0) {
            // if not, indicate failure
            return -1;
        }

        // shared user range in Phase 5
        const uintptr_t user_lo = PROC_START_ADDR;
        const uintptr_t user_hi = MEMSIZE_VIRTUAL;

        // must be inside user range
        if (addr < user_lo || addr >= user_hi) {
            return -1;
        }

        // disallow allocating over the console page
        if (round_down(addr, PAGESIZE) == CONSOLE_ADDR) {
            return -1;
        }

        // must not already be mapped
        x86_64_pagetable* pt = current->pagetable;
        vmiter it(pt, addr);
        if (it.present()) {
            return -1;
        }

        // allocate a fresh physical page (reserved by kalloc via refcount++)
        void* page = kalloc(PAGESIZE);
        if (!page) {
            // if not enough memory, indicate failure
            return -1; 
        }
        // get physical address of allocated page
        uintptr_t pa = kptr2pa(page);

        // map with permissions PTE_P | PTE_U | PTE_W, bc it's a new user page
        if (it.try_map(pa, PTE_P | PTE_U | PTE_W) < 0) {
            kfree(page);
            return -1;
        }

        // set all bytes in the new page to zero
        memset(pa2kptr(pa), 0, PAGESIZE);
        return 0;
    }

 
// choose a process to run then run it
void schedule() {
    pid_t pid = current->pid;
    for (unsigned spins = 1; true; ++spins) {
        pid = (pid + 1) % MAXNPROC;
        if (ptable[pid].state == P_RUNNABLE) {
            run(&ptable[pid]);
        }

        // If Control-C was typed, exit the virtual machine.
        check_keyboard();

        // If spinning forever, show the memviewer.
        if (spins % (1 << 12) == 0) {
            memshow();
        }
    }
}

// run process p by restoring its state and jumping back to user mode
void run(proc* p) {
    assert(p->state == P_RUNNABLE);
    current = p;

    // Check the process's current registers.
    check_process_registers(p);

    // Check the process's current pagetable.
    check_pagetable(p->pagetable);

    // assert that no user-writable pages are shared
    assert_no_shared_writable(p->pagetable);

    // This function is defined in k-exception.S. It restores the process's
    // registers then jumps back to user mode.
    exception_return(p);

    // should never get here
    while (true) {
    }
}

// called from exception handler to update memory display
void memshow() {
    static unsigned last_ticks = 0;
    static int showing = 0;

    // switch to a new process every 0.25 sec
    if (last_ticks == 0 || ticks - last_ticks >= HZ / 2) {
        last_ticks = ticks;
        showing = (showing + 1) % MAXNPROC;
    }

    proc* p = nullptr;
    for (int search = 0; !p && search < MAXNPROC; ++search) {
        if (ptable[showing].state != P_FREE
            && ptable[showing].pagetable) {
            p = &ptable[showing];
        } else {
            showing = (showing + 1) % MAXNPROC;
        }
    }

    console_memviewer(p);
    if (!p) {
        console_printf(CPOS(10, 26), CS_WHITE "   VIRTUAL ADDRESS SPACE\n"
            "                          [All processes have exited]\n"
            "\n\n\n\n\n\n\n\n\n\n\n");
    }
}