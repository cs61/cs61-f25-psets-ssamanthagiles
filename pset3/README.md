CS 61 Problem Set 3
===================

**Fill out both this file and `AUTHORS.md` before submitting.** We grade
anonymously, so put all personally identifying information, including
collaborators, in `AUTHORS.md`.

Grading notes (if any)
----------------------
The output of my test program may look a bit uncoordinated because the kernel’s memory-dump and tracing messages print at the same time as my user-level output. This causes some lines to appear out of order or mixed together, even though the system calls are working.


Extra credit attempted (if any)
-------------------------------
Completed intermediate check in (going to OH before 10/13)
For extra credit, I added a bunch of optional syscalls to my kernel and wrote my own test program to make sure they all worked. I got sys_uptime, sys_sleep, sys_random, sys_page_alloc, sys_page_free, and sys_kill all running, and then I made a custom file (p-testprogram.cc) that prints out uptime before and after sleeping, generates random numbers, tries allocating and freeing a page, and even tests killing another process.