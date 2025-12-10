CS 61 Problem Set 5
===================

**Fill out both this file and `AUTHORS.md` before submitting.** We grade
anonymously, so put all personally identifying information, including
collaborators, in `AUTHORS.md`.

Grading notes (if any)
----------------------
Added an apply_redirection() helper that consolidates the open/dup2/close sequence for input, output, and error redirections, and I implemented save_fds() and restore_fds() to eliminate repeated file-descriptor saving and restoring in the built-in cd path. After adding these helpers, I replaced the duplicated redirection blocks with clean function calls, which shortened the affected functions


Extra credit attempted (if any)
-------------------------------
I added support for SIGINT interruption and additional redirection forms beyond the base requirements. My shell places the entire foreground pipeline into a process group (pgid), stores that pgid globally, and forwards SIGINT (Ctrl-C) to the whole pipeline using kill(-pgid, SIGINT). This allows the shell itself to remain running while correctly interrupting only the active job.
I also extended redirection handling to support more complex cases including >>, 2>>, and combined redirections such as &> and &>>. These forms are parsed by the starter code but not required; I added full support so they now behave like in a POSIX shell.