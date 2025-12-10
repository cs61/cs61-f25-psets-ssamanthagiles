#include "u-lib.hh"

// panic, assert_fail
//     Call the SYSCALL_PANIC system call so the kernel loops until Control-C.

void panic(const char* format, ...) {
    va_list val;
    va_start(val, format);
    char buf[240];
    int len = vsnprintf(buf, sizeof(buf) - 1, format, val);
    va_end(val);
    if (len > 0 && buf[len - 1] != '\n') {
        strcpy(buf + len - (len == (int) sizeof(buf) - 1), "\n");
    }
    sys_panic(buf);
}

void error_vprintf(const char* format, va_list val) {
    int scroll_mode = console_printer::scroll_blank;
    console_printer pr(-1, scroll_mode);
    if (pr.cell_ < console + END_CPOS - CONSOLE_COLUMNS) {
        pr.cell_ = console + END_CPOS;
    }
    pr.vprintf(format, val);
    pr.move_cursor();
}

void assert_fail(const char* file, int line, const char* msg,
                 const char* description) {
    cursorpos = CPOS(23, 0);
    char buf[240];
    if (description) {
        snprintf(buf, sizeof(buf),
                 "%s:%d: %s\n%s:%d: user assertion '%s' failed\n",
                 file, line, description, file, line, msg);
    } else {
        snprintf(buf, sizeof(buf),
                 "%s:%d: user assertion '%s' failed\n",
                 file, line, msg);
    }
    sys_panic(buf);
}

// syscall wrappers (for ec)

long syscall(long num, long arg1, long arg2, long arg3);


// uptime()
long uptime() {
    return syscall(SYSCALL_UPTIME, 0, 0, 0);
}

// sleep(ticks)
long sleep(long duration) {
    return syscall(SYSCALL_SLEEP, duration, 0, 0);
}

// random()
long random() {
    return syscall(SYSCALL_RANDOM, 0, 0, 0);
}

// random_kernel()  -- only if you actually have SYSCALL_RANDOMKERNEL in lib.hh
long random_kernel() {
    return syscall(SYSCALL_RANDOMKERNEL, 0, 0, 0);
}

// kill(pid)
long kill(int pid) {
    return syscall(SYSCALL_KILL, pid, 0, 0);
}

// free a user page
long page_free(uintptr_t addr) {
    return syscall(SYSCALL_PAGE_FREE, addr, 0, 0);
}