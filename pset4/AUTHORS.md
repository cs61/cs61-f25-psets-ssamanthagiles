Author and collaborators
========================

Primary student
---------------
Samantha Giles


Collaborators
-------------
Faisal

Citations
---------
Used github copilot to complete problem set

Used these resources to complete the pset:
“File Descriptors and System Calls in Unix/Linux” — e.g., man pages: open, read, write, lseek.
“Buffered I/O vs Unbuffered I/O in C” — tutorial discussing how stdio uses buffers and what happens if you issue many small system calls.
“Memory-Mapped Files (mmap) vs Read/Write System Calls” — blog/StackOverflow thread comparing performance and trade-offs.
“Cache Control and Prefetching for File I/O” — article exploring posix_fadvise, madvise, and how to reduce system call overhead.
“Seekable vs Non-seekable File Descriptors” — documentation on how pipes, sockets, /dev/urandom, terminals behave differently with lseek.
“Handling Short Reads and Short Writes in POSIX” — guide on dealing with read() or write() returning fewer bytes than requested, and how to loop to complete I/O.
“Performance Testing I/O: Strace, Iostat, and Microbenchmarks” — a how-to on using strace -o strace.out, timing, and comparing to stdio.
“Large File I/O Patterns: Sequential vs Reverse vs Strided Access” — lecture/explainer on how access patterns affect cache, OS read-ahead, and performance.
“Buffered Write Implementation: Flushing, Write Buffers, and Ensuring Correctness on Seek/Writes” — detailed blog or textbook chapter about implementing write caching while preserving semantics.
“Assertions and Performance Implications in C++” — article on how enabling/disabling asserts (NDEBUG) affects performance in critical code.