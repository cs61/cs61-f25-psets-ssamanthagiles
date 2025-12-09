#include "sh61.hh"
#include <cstring>
#include <cerrno>
#include <vector>
#include <sys/stat.h>
#include <sys/wait.h>


#undef exit
#define exit __DO_NOT_CALL_EXIT__READ_PROBLEM_SET_DESCRIPTION__

// understanding what syscalls return:
    // return -1 on error, set errno
    // return 0 or non-negative value on success, depending on syscall

// understanding CS61 shell parser:
    // TYPE_AND = &&
    // TYPE_OR = ||
    // TYPE_SEQUENCE = ;
    // TYPE_PIPE = |

// command structure 
struct command {

    // vector to hold arguments of command for execvp
    // NOT redirections or filenames used for redirections
    std::vector<std::string> args;
    // process ID running this command, -1 if none
    pid_t pid = -1;

    // phase 7, redirections
    std::string infile = ""; // file for input redirection, "" if none
    std::string outfile = ""; // file for output redirection, "" if none   
    std::string errfile = ""; // file for error redirection, "" if none

    command();
    ~command();

    // run the command in a child process
    void run();
};

// phase 3 forward declarations for helper functions:

// run a single simple command (echo hello)
int run_command(command_parser cmdpar);

// run a pipeline (handles | )
int run_pipeline(pipeline_parser pipepar);

// run a conditional chain (handles && and ||)
int run_conditional(conditional_parser condit);

// command::command()
//    This constructor function initializes a `command` structure. 
command::command() {
}

// command::~command()
//    This destructor function is called to delete a command.
command::~command() {
}

// COMMAND EXECUTION
void command::run() {

    // ensure command is not already running
    // when pid > 0, command is already running
    assert(this->pid == -1);


    // ensure there is at least one argument in the command
    // argument = executable name + args
    assert(this->args.size() > 0);

    // argv preparation for execvp
    
    // execvp is the syscall that replaces the current process image with the specified program
    // proc image = code, data, stack, heap, fd, etc. of a process

    // execvp can only read char* arguments !
    std::vector<char*> argv;

    // allocate arguments to vector if arguments exist
    for (auto& arg : this->args) {
        // convert from std::string to char* to pass to execvp
        // originally arguments are stored as std::string in command struct
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    // after allocation, terminate argument list with nullptr
    argv.push_back(nullptr); 

    // fork a new process and stored its pid
    pid_t child_pid = fork();
    
    // if child_pid return value is < 0
    if (child_pid < 0) {
        // fork failed
        perror("fork");
        // exit shell
        _exit(EXIT_FAILURE);
    
    // if fork return value is 0, then in child process
    } else if (child_pid == 0) {

        // check for input redirections (<) of a file
        // if not empty,
        if (!infile.empty()) {
            // then there is an infile redirection
            // open infile in read only mode
            int fd = open(infile.c_str(), O_RDONLY);
            // if opening file descriptor fails
            if (fd < 0) {
                perror(infile.c_str());
                // exit child process (not whole shell)
                _exit(1);
            }
            // dup2 is used to duplicate file descriptors
            // redirect stdin to infile
            dup2(fd, STDIN_FILENO);
            // now close after handling infile redirection
            close(fd);
        }

        // check for output redirections (>) of a file
        // if not empty,
        if (!outfile.empty()) {
            // then there is an outfile redirection
            // open outfile in write only mode, create if doesn't exist, truncate if exists
            int fd = open(outfile.c_str(),
                          O_WRONLY | O_CREAT | O_TRUNC,
                          0666);
            // if opening file descriptor fails
            if (fd < 0) {
                perror(outfile.c_str());
                // exit child process (not whole shell)
                _exit(1);
            }
            // redirect stdout to outfile
            dup2(fd, STDOUT_FILENO);
            // now close after handling outfile redirection
            close(fd);
        }

        // check for error redirections (2>) of a file
        // if not empty,
        if (!errfile.empty()) {
            // then there is an errfile redirection
            // open errfile in write only mode, create if doesn't exist, truncate if exists
            int fd = open(errfile.c_str(),
                          O_WRONLY | O_CREAT | O_TRUNC,
                          0666);
            // if opening file descriptor fails
            if (fd < 0) {
                perror(errfile.c_str());
                // exit child process (not whole shell)
                _exit(1);
            }
            // redirect stderr to errfile
            dup2(fd, STDERR_FILENO);
            // now close after handling errfile redirection
            close(fd);
        }

        // now execute the command in the child process
        execvp(argv[0], argv.data());
        // if execvp returns instead of replacing process image, there was an error
        perror("execvp");
        // exit child process (not whole shell)
        _exit(EXIT_FAILURE);
    } else {
        // in parent process, store child pid in command struct
        this->pid = child_pid;
    }
}

// run a single simple command (ex: echo hello)
int run_command(command_parser cmdpar) {
    // create new command struct dynamically on heap
    command* c = new command;
    
     // temp variables for redirection filenames
     std::string infile, outfile, errfile;

     // tok holds current token (argument or redirect operator) in command parser
     auto tok = cmdpar.token_begin();
     // loop until the end of command parser
     while (tok != cmdpar.end()) {
 
         // if token is a redirect operator
         if (tok.type() == TYPE_REDIRECT_OP) {
             // get the redirect operator string
             std::string op = tok.str(); // "<", ">", or "2>"
             // now move token to get filename
             ++tok;
             // if no filename after redirect operator or token is not normal
             if (tok == cmdpar.end() || tok.type() != TYPE_NORMAL) {
                 fprintf(stderr, "Missing filename after redirect\n");
                 // exit child process ONLY (exiting shell is handled in parent)
                 _exit(1);
             }
             
             // get the filename string
             std::string fname = tok.str();
             
             // if operator is <, >, or 2>, set appropriate redirection filename
             // based on temp variables declared above that will be stored in command struct
             if (op == "<")       c->infile = fname;
             else if (op == ">")  c->outfile = fname;
             else if (op == "2>") c->errfile = fname;
             
             // iterate over to next token
             ++tok;
             continue;
         }
 
         // else, token is not redirection operator, so it is a normal argument
         // add argument to command's args vector
         c->args.push_back(tok.str());
         // increment token to next
         ++tok;
     }
     
    // phase 8, handle cd before running command
    // cd = change directory
    // as long as there are arguments in the command and first argument is "cd"
    if (c->args.size() > 0 && c->args[0] == "cd") {

        // save original file descriptors to restore later
        // using dup syscall
        int old_stdin  = dup(STDIN_FILENO);
        int old_stdout = dup(STDOUT_FILENO);
        int old_stderr = dup(STDERR_FILENO);

        // check for redirections before executing cd command

        // if there is an input redirection
        if (!c->infile.empty()) {
            // open infile in read only mode
            int fd = open(c->infile.c_str(), O_RDONLY);
            // if opening file descriptor fails
            if (fd < 0) {
                // print error
                perror(c->infile.c_str());
                // duplicate old fds to restore
                dup2(old_stderr, STDERR_FILENO);
                dup2(old_stdout, STDOUT_FILENO);
                dup2(old_stdin,  STDIN_FILENO);
                // after close restored fds
                close(old_stderr);
                close(old_stdout);
                close(old_stdin);
                // need to delete command struct bc created dynamically on heap
                delete c;
                // indicate failure bc opening infile failed
                return 1;
            }
            // else, opening infile succeeded
            // redirect stdin to infile
            dup2(fd, STDIN_FILENO);
            // now close after handling infile redirection
            close(fd);
        }

        // if there is an output redirection
        if (!c->outfile.empty()) {
            // open outfile in write only mode, create if doesn't exist, truncate if exists
            int fd = open(c->outfile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
            // if opening file descriptor fails
            if (fd < 0) {
                // print error
                perror(c->outfile.c_str());
                // duplicate old fds to restore
                dup2(old_stderr, STDERR_FILENO);
                dup2(old_stdout, STDOUT_FILENO);
                dup2(old_stdin,  STDIN_FILENO);
                // after close restored fds
                close(old_stderr);
                close(old_stdout);
                close(old_stdin);
                // delete command struct bc created dynamically on heap
                delete c;
                // indicate failure bc opening outfile failed
                return 1;
            }
            // else, opening outfile succeeded
            // redirect stdout to outfile
            dup2(fd, STDOUT_FILENO);
            // now close after handling outfile redirection
            close(fd);
        }

        // if there is an error redirection
        if (!c->errfile.empty()) {
            // open errfile in write only mode, create if doesn't exist, truncate if exists
            int fd = open(c->errfile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
            // if opening file descriptor fails
            if (fd < 0) {
                // print error
                perror(c->errfile.c_str());
                // duplicate old fds to restore
                dup2(old_stderr, STDERR_FILENO);
                dup2(old_stdout, STDOUT_FILENO);
                dup2(old_stdin,  STDIN_FILENO);
                // after close restored fds
                close(old_stderr);
                close(old_stdout);
                close(old_stdin);
                // delete command struct bc created dynamically on heap
                delete c;
                // indicate failure bc opening errfile failed
                return 1;
            }
            // else, opening errfile succeeded
            // redirect stderr to errfile
            dup2(fd, STDERR_FILENO);
            // now close after handling errfile redirection
            close(fd);
        }

        // now can execute cd command
        // path holds the directory to change to
        const char* path = nullptr;

        // if only "cd" with no arguments
        if (c->args.size() == 1) {
            // get HOME environment variable for path
            // bc cd with no args goes to home directory
            path = getenv("HOME");
            // if HOME environment variable not set
            // then default to current directory "."
            if (!path) path = ".";
        // or there were arguments after "cd"
        } else {
            // so set path to the first argument after "cd"
            path = c->args[1].c_str();
        }

        // rc holds return code of chdir syscall
        // chdir changes the current working directory of the process
        int rc = 0;

        // after determining path, attempt to change directory

        // if chdir fails (returns -1)
        if (chdir(path) < 0) {
            // print error for changing directory
            perror(path);
            // indicate failure
            rc = 1;
        }

        // else chdir succeeded, rc remains 0
        
        // restore original file descriptors after cd command
        // using dup2 syscall
        dup2(old_stderr, STDERR_FILENO);
        dup2(old_stdout, STDOUT_FILENO);
        dup2(old_stdin,  STDIN_FILENO);
        // after close restored fds
        close(old_stderr);
        close(old_stdout);
        close(old_stdin);

        // need to delete command struct bc created dynamically on heap
        delete c;
        // return return code of cd command
        return rc;
    }

    // run the command in a child process
    c->run();
    // declare status outside if scope so it can be used after
    // holds current status of child process during waitpid
    int status;

    // wait for child to finish executing command 
    if (c->pid > 0) {
        // if waitpid fails (when command is interrupted by signal)
        if (waitpid(c->pid, &status, 0) < 0) {
            // indicate error
            perror("waitpid");
            // exit shell
            _exit(EXIT_FAILURE);
        }
    }
    // need to delete command struct bc created dynamically on heap
    // so need to explicitly free memory
    delete c;

    // convert waitpid status to WEXITSTATUS
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    } else {
        // not EXITFAILURE bc EXITFAILURE doesn't guarantee 1 is returned
        return 1; 
    }
}

// handles | in pipelines
int run_pipeline(pipeline_parser pipepar) {

    // count number of commands in pipeline
    int n = 0;

    // loop over commands in pipeline parser
    for (auto cmd = pipepar.command_begin(); cmd != pipepar.end(); ++cmd) {
        // after looping, increment count of commands
        n++;
    }

    // if only ONE command
    if (n == 1) {
        // just run that command normally bc no pipes needed
        return run_command(pipepar.command_begin());
    }

    // pipeline with n commands has n-1 pipes
    // each pipe has 2 file descriptors: [0] for reading, [1] for writing
    // this vector holds all pipes for the pipeline
    std::vector<int[2]> pipes(n - 1);
    // as long as there are n-1 pipes, create them
    for (int i = 0; i < n - 1; i++) {
        // if pipe syscall returns -1 
        if (pipe(pipes[i]) < 0) {
            // indicate error 
            perror("pipe");
            // exit shell
            _exit(EXIT_FAILURE);
        }
    }

    // index of current command in the pipeline
    int cmd_index = 0;

    // pid means process id
    // last_pid holds pid of last command in the pipeline during forking
    // initialize to -1 to indicate no pid yet
    pid_t last_pid = -1;

    // loop over commands in the pipeline
    for (auto cmdpar = pipepar.command_begin(); cmdpar != pipepar.end();
         ++cmdpar, ++cmd_index) {
        
        // fork a new process for this command
        pid_t pid = fork();
        
        // if fork failed (meaning pid remains -1)
        if (pid < 0) {
            // indicate error
            perror("fork");
            // exit shell
            _exit(EXIT_FAILURE);
        }

        // if fork succeeded
        if (pid == 0) {
            
            // vector that holds arguments for execvp
            std::vector<std::string> args;

            // declaring temporary variables for redirections
            std::string infile, outfile, errfile;

            // as long as there are tokens in the command
            // tokens (arguments and redirection operators)
            for (auto tok = cmdpar.token_begin(); tok != cmdpar.token_end(); ++tok) {
                
                // if token is redirection operator
                if (tok.type() == TYPE_REDIRECT_OP) {
                    // op holds the redirect operator string (<, >, or 2>)
                    std::string op = tok.str(); 
                    // increment tok to get filename
                    ++tok;

                    // handle error cases for redirection
                    // if no filename after redirect operator or token is not normal
                    if (tok == cmdpar.token_end() || tok.type() != TYPE_NORMAL) {
                        // print error bc after redirect operator must be a filename
                        fprintf(stderr, "Missing filename after redirect\n");
                        // exit child process ONLY (exiting shell is handled in parent)
                        _exit(1);
                    }

                    // fname holds the filename string bc successfully got filename token
                    std::string fname = tok.str();

                    // set appropriate redirection filename based on operator
                    // handle <, >, and 2> cases
                    if (op == "<")       infile = fname;
                    else if (op == ">")  outfile = fname;
                    else if (op == "2>") errfile = fname;
                // else token is normal argument (not redirection operator)
                } else {
                    // and can add argument to args vector to be used for execvp
                    // which only takes normal arguments (no redirection operators)
                    // args = still string
                    // argv = char*
                    args.push_back(tok.str());
                }
            }

            // if there are no arguments in this command
            if (args.empty()) {
                // nothing to execute, so just exit child process
                _exit(0);
            }

            // vector to hold the arguments for execvp as char*
            std::vector<char*> argv;

            // allocate arguments to argv vector
            for (auto& s : args) {
                // convert from std::string to char* to pass to execvp
                argv.push_back(const_cast<char*>(s.c_str()));
            }
            // after allocation, terminate argument list with nullptr
            argv.push_back(nullptr);

            // set up pipeline ends (stdin/stdout)
            // 0 = first command
            // if first command in pipeline
            if (cmd_index == 0) {

                // redirect stdout to pipe[0][1]
                // if dup2 fails
                if (dup2(pipes[0][1], STDOUT_FILENO) < 0) {
                    // indicate error
                    perror("dup2");
                    // exit shell
                    _exit(EXIT_FAILURE);
                }
            // if last command in pipeline
            } else if (cmd_index == n - 1) {
                // redirect stdin to previous pipe
                // if dup2 fails
                if (dup2(pipes[n - 2][0], STDIN_FILENO) < 0) {
                    // indicate error
                    perror("dup2");
                    // exit shell
                    _exit(EXIT_FAILURE);
                }
            // if it's a middle command in the pipeline
            } else {
                // redirect stdin to previous pipe's read end
                // and stdout to current pipe's write end
                // if either dup2 fails
                if (dup2(pipes[cmd_index - 1][0], STDIN_FILENO) < 0
                 || dup2(pipes[cmd_index][1],   STDOUT_FILENO) < 0) {
                    // indicate error
                    perror("dup2");
                    // exit shell
                    _exit(EXIT_FAILURE);
                }
            }

            // now apply redirections (these OVERRIDE the pipeline on that fd)

            // INPUT redirection (< file)
            // if infile is not empty,
            if (!infile.empty()) {

                // then open infile in read only mode
                int fd = open(infile.c_str(), O_RDONLY);

                // if opening file descriptor fails
                if (fd < 0) {
                    perror(infile.c_str());
                    // exit child process (not whole shell)
                    _exit(1);
                }

                // redirect stdin to infile
                // if duplicating the fd fails
                if (dup2(fd, STDIN_FILENO) < 0) {
                    perror("dup2");
                    // exit child process (not whole shell)
                    _exit(1);
                }
                // now close after handling infile redirection
                close(fd);
            }

            // OUTPUT redirection (> file)
            // if outfile is not empty,
            if (!outfile.empty()) {
                // open outfile in write only mode, create if doesn't exist, truncate if exists
                int fd = open(outfile.c_str(),
                              O_WRONLY | O_CREAT | O_TRUNC,
                              0666);
                // if opening file descriptor fails
                if (fd < 0) {
                    perror(outfile.c_str());
                    // exit child process (not whole shell)
                    _exit(1);
                }
                // redirect stdout to outfile
                // if duplicating the fd fails
                if (dup2(fd, STDOUT_FILENO) < 0) {
                    perror("dup2");
                    // exit child process (not whole shell)
                    _exit(1);
                }
                // close after handling outfile redirection
                close(fd);
            }

            // STDERR redirection (2> file)
            // if errfile is not empty,
            if (!errfile.empty()) {
                // open errfile in write only mode, create if doesn't exist, truncate if exists
                int fd = open(errfile.c_str(),
                              O_WRONLY | O_CREAT | O_TRUNC,
                              0666);
                // if opening file descriptor fails
                if (fd < 0) {
                    perror(errfile.c_str());
                    // exit child process (not whole shell)
                    _exit(1);
                }
                // redirect stderr to errfile
                // if duplicating the fd fails
                if (dup2(fd, STDERR_FILENO) < 0) {
                    perror("dup2");
                    // exit child process (not whole shell)
                    _exit(1);
                }
                // close after handling errfile redirection
                close(fd);
            }

            // close all pipe fds in child after checking redirections
            for (int i = 0; i < n - 1; i++) {
                // [i] = pipe number
                // [0] = read end
                // [1] = write end
                close(pipes[i][0]);
                close(pipes[i][1]);
            }

            // execute the command in this child process through execvp
            execvp(argv[0], argv.data());
            // if execvp returns, there was an error
            perror("execvp");
            // exit shell
            _exit(EXIT_FAILURE);
        }

        // then fork succeeded, so back in parent process

        else {
           // continue to next command in pipeline to fork...
            if (cmd_index == n - 1) {
                // update last_pid to pid of last command that was forked
                last_pid = pid;
            }
        }
    }

    // parent has to close ALL pipe file descriptors after forking all children
    for (int i = 0; i < n - 1; i++) {
        // close both ends of each pipe
        // [i] = pipe number
        // [0] = read end
        // [1] = write end
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    // parent has to wait for last command in pipeline to finish
    // status of pipeline is status of last command of child processes
    int status;
    // if waitpid fails
    if (waitpid(last_pid, &status, 0) < 0) {
        // indicate error
        perror("waitpid");
        // exit shell
        _exit(EXIT_FAILURE);
    }

    // convert waitpid status to WEXITSTATUS after waiting for all children
    if (WIFEXITED(status)) {
        // return exit status of last command in pipeline
        return WEXITSTATUS(status);
    }
    // treat abnormal termination as failure
    return 1;
}


// handles && and ||
int run_conditional(conditional_parser condit) {

    // holds last status of the pipeline (return value)
    int last_status = 0;

    // handles first command in conditional chain
    bool first = true;

    // operator that came before the current command
    // (there is no operator before the first command)
    int prev_op = TYPE_SEQUENCE; 

    // loop over pipelines inside this conditional
    // ** each pipeline is separated by && or ||
    for (auto pipepar = condit.pipeline_begin(); pipepar != condit.end(); ++pipepar) {

        
         // handle whether to run this command based on previous status and operator
         bool should_run = true;

         // if not the first command in the chain
         // (first command always runs)
         if (!first) {

             // and if previous op was && and last_status != 0 (previous failed)
             if (prev_op == TYPE_AND && last_status != 0) {
                 // (&& means if the previous command failed, skip this one)
                 // so don't run this command
                 should_run = false;
             }
 
             // and if previous op was || and last_status == 0 (previous succeeded)
             else if (prev_op == TYPE_OR && last_status == 0) {
                 // (|| means if the previous command succeeded, skip this one)
                 // so don't run this command
                 should_run = false;
             }
         }
         
         // if should_run is still true
         if (should_run) {
             // THEN run this command and update last_status
             last_status = run_pipeline(pipepar);
         }
         
         // else, don't run command and last_status remains the same
 
         // update prev_op based on the operator that comes AFTER this command
         prev_op = pipepar.next_op();
 
         // always turn off first flag 
         first = false;
     }
 
     // status of the last command that WAS run (or last_status carried through skips)
     return last_status;
 }


void run_line(command_line_parser clp) {
    
    // loop over conditionals separated by ;
    for (auto condit = clp.conditional_begin(); condit != clp.end(); ++condit){

        // if the next operator is & 
        if (condit.next_op() == TYPE_BACKGROUND) {
            // fork the condtional chain to run in background and store in bg
            pid_t bg = fork();
            // if fork failed
            if (bg < 0) {
                // indicate error
                perror("fork");
                // exit shell
                _exit(EXIT_FAILURE);
            }
            // if fork succeeded 
            if (bg == 0) {
                // run the conditional in the child process
                int status = run_conditional(condit);
                // exit with the status of the conditional
                _exit(status);
            }

            // parent process just
            continue; // (s)
        }

        // just run the conditional normally if the next op is not &
        // bc ";" means run in foreground
        run_conditional(condit);
    }
}


int main(int argc, char* argv[]) {
    FILE* command_file = stdin;
    bool quiet = false;

    // Check for `-q` option: be quiet (print no prompts)
    if (argc > 1 && strcmp(argv[1], "-q") == 0) {
        quiet = true;
        --argc, ++argv;
    }

    // Check for filename option: read commands from file
    if (argc > 1) {
        command_file = fopen(argv[1], "rb");
        if (!command_file) {
            perror(argv[1]);
            return 1;
        }
    }

    // - Put the shell into the foreground
    // - Ignore the SIGTTOU signal, which is sent when the shell is put back
    //   into the foreground
    claim_foreground(0);
    set_signal_handler(SIGTTOU, SIG_IGN);

    char buf[BUFSIZ];
    int bufpos = 0;
    bool needprompt = true;

    while (!feof(command_file)) {
        // Print the prompt at the beginning of the line
        if (needprompt && !quiet) {
            printf("sh61[%d]$ ", getpid());
            fflush(stdout);
            needprompt = false;
        }

        // Read a string, checking for error or EOF
        if (fgets(&buf[bufpos], BUFSIZ - bufpos, command_file) == nullptr) {
            if (ferror(command_file) && errno == EINTR) {
                // ignore EINTR errors
                clearerr(command_file);
                buf[bufpos] = 0;
            } else {
                if (ferror(command_file)) {
                    perror("sh61");
                }
                break;
            }
        }

        // If a complete command line has been provided, run it
        bufpos = strlen(buf);
        if (bufpos == BUFSIZ - 1 || (bufpos > 0 && buf[bufpos - 1] == '\n')) {
            run_line(command_line_parser{buf});
            bufpos = 0;
            needprompt = 1;
        
            // phase 6, reap zombie processes
            // reap means to clean up after terminated child processes
            // status holds status of reaped child process
            int status;

            // if waitpid is -1, means wait for any child process
            // WNOHANG = non-blocking, only checks for terminated children so 
                // it doesn't block the shell if no children have terminated
            while (waitpid(-1, &status, WNOHANG) > 0) {
                // when waitpid returns > 0, means reaped a zombie
            }
        }
    }
    // close command file if not stdin
    return 0;
}
