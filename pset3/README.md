CS 61 Problem Set 3
===================

**Fill out both this file and `AUTHORS.md` before submitting.** We grade
anonymously, so put all personally identifying information, including
collaborators, in `AUTHORS.md`.

Grading notes (if any)
----------------------


Extra credit attempted (if any)
-------------------------------
Completed intermediate check in (going to OH before 10/13)

For extra credit, I implemented several system calls in my WeensyOS kernel and wrote a custom user test program (p-testprogram.cc) to verify each one. My program calls sys_uptime, which returns the number of ticks since boot; sys_sleep, which pauses the calling process for a specified number of ticks; and sys_random, which provides a kernel-generated pseudorandom value. I also implemented sys_page_alloc and sys_page_free to test user-level page management by allocating a page at a valid virtual address, detecting double-allocations, and confirming correct freeing behavior. Finally, I added sys_kill, which attempts to terminate a process by PID and correctly rejects invalid or self-targeted kill attempts. To run my test, you can rebuild and launch the OS in console mode with make clean, make, and make run-console; my program runs automatically as the first process and prints its results on startup. 