CS 61 Problem Set 4
===================

**Fill out both this file and `AUTHORS.md` before submitting.** We grade
anonymously, so put all personally identifying information, including
collaborators, in `AUTHORS.md`.

Grading notes (if any)
----------------------
Fixed style of my code (originally 5/10)
Cleaned up indents 
The mode vs. write_mode - mode is the original open mode (whether the OS opened it read-only or write-only). write_mode is the internal state variable to track whether im  currently writing or reading in buffering logic.

For the seek question, my implementation does not rely on the lseek succeeding; it is only used when possible to align offsets. Since I already check for ESPIPE and handle non-seekable files correctly, no functional change was needed.

I changed size_t copy_sz = std::min(space, sz - nwritten);
to 
size_t copy_sz = (space < (sz - nwritten) ? space : (sz - nwritten));

Fixed chunk too !
size_t chunk = (size_t) std::min<off_t>(remain, (off_t) sizeof tmp);
to 
size_t chunk = (remain < (off_t)sizeof tmp ? remain : (off_t)sizeof tmp);

Extra credit attempted (if any)
-------------------------------
Created 5 more tests for extra credit
1. interleave61
2. pingpong61
3. randseek61
4. misalignwrite61
5. zigzagseek61
to run make ___ (test)
./___(test)
