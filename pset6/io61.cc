#include "io61.hh"
#include <climits>
#include <cerrno>
#include <shared_mutex>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <mutex>
#include <vector>

// io61.cc

// documenting transfer of locks structure
// lock(A)
// lock(B)
// transfer
// unlock(A)
// unlock(B)

// coarse-grained -> big area locks
// fine-grained -> small area locks, specific regions


// io61_file
struct io61_file {
    int fd;
    int mode;
    bool seekable;

    // Cache fields
    enum { cbufsz = 8192 };
    unsigned char cbuf[cbufsz];
    off_t tag;
    off_t pos_tag;
    off_t end_tag;

    // Positioned mode
    bool positioned;
    bool dirty;
     // protects all range-lock metadata
     std::mutex m;
     // used to sleep/wake threads waiting on locks
     std::condition_variable_any cv;
 
     // split file into regions for fine-grained locking
     static const int NREG = 128;
     static const off_t REGION_SIZE = 8192 * 16;
 
     struct region_lock {
         unsigned locked = 0;        // 0 = unlocked, >0 = locked by `owner`
         std::thread::id owner = std::thread::id();  // empty "no thread";      // which thread owns this region
     };
 
     region_lock reg[NREG];          // lock state for each region
 };

// ragelock struct to represent a range lock
static int file_region(off_t off) {
    return off / io61_file::REGION_SIZE;
}

// overlap checking helper function
bool may_overlap_with_other_lock(io61_file* f, off_t off, off_t len) {
    std::thread::id me = std::this_thread::get_id();

    int rstart = file_region(off);
    int rend   = file_region(off + len - 1);

    if (rstart < 0) rstart = 0;
    if (rend >= io61_file::NREG) rend = io61_file::NREG - 1;

    for (int ri = rstart; ri <= rend; ++ri) {
        if (f->reg[ri].locked > 0 &&
            f->reg[ri].owner != me) {
            return true;   // another thread owns this region
        }
    }
    return false;
}



// forward dec of flush helper function
static int io61_flush_dirty_positioned(io61_file* f);

// switch from positioned to non-positioned mode
static int io61_maybe_unposition(io61_file* f) {
    if (!f->positioned) {
        return 0;
    }   
    if (f->dirty){
        if (io61_flush_dirty_positioned(f) == -1) {
            return -1;
        }
    }
    
    if (f->seekable) {
        if (lseek(f->fd, f->pos_tag, SEEK_SET) == -1) {
            return -1;
        }
    }

    // drop positioned cache; start with an empty non-positioned cache
    f->tag = f->end_tag = f->pos_tag;
    f->positioned = false;
    f->dirty = false;

    return 0;
}

// Forward declarations for locking functions
int io61_try_lock(io61_file* f, off_t off, off_t len, int locktype);
int io61_lock(io61_file* f, off_t off, off_t len, int locktype);
int io61_unlock(io61_file* f, off_t off, off_t len);

// io61_fdopen(fd, mode)
//    Returns a new io61_file for file descriptor `fd`. `mode` is either
//    O_RDONLY for a read-only file, O_WRONLY for a write-only file,
//    or O_RDWR for a read/write file.

io61_file* io61_fdopen(int fd, int mode) {
    assert(fd >= 0);
    assert((mode & O_APPEND) == 0);
    io61_file* f = new io61_file;
    f->fd = fd;
    f->mode = mode & O_ACCMODE;
    off_t off = lseek(fd, 0, SEEK_CUR);
    if (off != -1) {
        f->seekable = true;
        f->tag = f->pos_tag = f->end_tag = off;
    } else {
        f->seekable = false;
        f->tag = f->pos_tag = f->end_tag = 0;
    }
    f->dirty = f->positioned = false;

    io61_maybe_unposition(f);

    return f;
}


// io61_close(f)
//    Closes the io61_file `f` and releases all its resources.

int io61_close(io61_file* f) {
    io61_flush(f);
    int r = close(f->fd);
    delete f;
    return r;
}


// NORMAL READING AND WRITING FUNCTIONS

// io61_readc(f)
//    Reads a single (unsigned) byte from `f` and returns it. Returns EOF,
//    which equals -1, on end of file or error.

static int io61_fill(io61_file* f);

int io61_readc(io61_file* f) {

    io61_maybe_unposition(f);
    
    assert(!f->positioned);
    if (f->pos_tag == f->end_tag) {
        io61_fill(f);
        if (f->pos_tag == f->end_tag) {
            return -1;
        }
    }
    unsigned char ch = f->cbuf[f->pos_tag - f->tag];
    ++f->pos_tag;
    return ch;
}


// io61_read(f, buf, sz)
//    Reads up to `sz` bytes from `f` into `buf`. Returns the number of
//    bytes read on success. Returns 0 if end-of-file is encountered before
//    any bytes are read, and -1 if an error is encountered before any
//    bytes are read.
//
//    Note that the return value might be positive, but less than `sz`,
//    if end-of-file or error is encountered before all `sz` bytes are read.
//    This is called a “short read.”

ssize_t io61_read(io61_file* f, unsigned char* buf, size_t sz) {
    
    // first, switch to non-positioned mode if necessary
    io61_maybe_unposition(f);

    assert(!f->positioned);
    size_t nread = 0;
    while (nread != sz) {
        if (f->pos_tag == f->end_tag) {
            int r = io61_fill(f);
            if (r == -1 && nread == 0) {
                return -1;
            } else if (f->pos_tag == f->end_tag) {
                break;
            }
        }
        size_t nleft = f->end_tag - f->pos_tag;
        size_t ncopy = std::min(sz - nread, nleft);
        memcpy(&buf[nread], &f->cbuf[f->pos_tag - f->tag], ncopy);
        nread += ncopy;
        f->pos_tag += ncopy;
    }
    return nread;
}


// io61_writec(f)
//    Write a single character `c` to `f` (converted to unsigned char).
//    Returns 0 on success and -1 on error.

int io61_writec(io61_file* f, int c) {
    io61_maybe_unposition(f);
    assert(!f->positioned);
    if (f->pos_tag == f->tag + f->cbufsz) {
        int r = io61_flush(f);
        if (r == -1) {
            return -1;
        }
    }
    f->cbuf[f->pos_tag - f->tag] = c;
    ++f->pos_tag;
    ++f->end_tag;
    f->dirty = true;
    return 0;
}


// io61_write(f, buf, sz)
//    Writes `sz` characters from `buf` to `f`. Returns `sz` on success.
//    Can write fewer than `sz` characters when there is an error, such as
//    a drive running out of space. In this case io61_write returns the
//    number of characters written, or -1 if no characters were written
//    before the error occurred.

ssize_t io61_write(io61_file* f, const unsigned char* buf, size_t sz) {
    io61_maybe_unposition(f);
    assert(!f->positioned);
    size_t nwritten = 0;
    while (nwritten != sz) {
        if (f->end_tag == f->tag + f->cbufsz) {
            int r = io61_flush(f);
            if (r == -1 && nwritten == 0) {
                return -1;
            } else if (r == -1) {
                break;
            }
        }
        size_t nleft = f->tag + f->cbufsz - f->pos_tag;
        size_t ncopy = std::min(sz - nwritten, nleft);
        memcpy(&f->cbuf[f->pos_tag - f->tag], &buf[nwritten], ncopy);
        f->pos_tag += ncopy;
        f->end_tag += ncopy;
        f->dirty = true;
        nwritten += ncopy;
    }
    return nwritten;
}


// io61_flush(f)
//    If `f` was opened for writes, `io61_flush(f)` forces a write of any
//    cached data written to `f`. Returns 0 on success; returns -1 if an error
//    is encountered before all cached data was written.
//
//    If `f` was opened read-only and is seekable, `io61_flush(f)` drops any
//    data cached for reading and seeks to the logical file position.

static int io61_flush_dirty(io61_file* f);
static int io61_flush_dirty_positioned(io61_file* f);
static int io61_flush_clean(io61_file* f);

int io61_flush(io61_file* f) {
    if (f->dirty && f->positioned) {
        return io61_flush_dirty_positioned(f);
    } else if (f->dirty) {
        return io61_flush_dirty(f);
    } else {
        return io61_flush_clean(f);
    }
}


// io61_seek(f, off)
//    Changes the file pointer for file `f` to `off` bytes into the file.
//    Returns 0 on success and -1 on failure.

int io61_seek(io61_file* f, off_t off) {

    if (io61_maybe_unposition(f) == -1) {
        return -1;
    }

    int r = io61_flush(f);
    if (r == -1) {
        return -1;
    }
    off_t roff = lseek(f->fd, off, SEEK_SET);
    if (roff == -1) {
        return -1;
    }
    f->tag = f->pos_tag = f->end_tag = off;
    f->positioned = false;
    return 0;
}


// Helper functions

// io61_fill(f)
//    Fill the cache by reading from the file. Returns 0 on success,
//    -1 on error. Used only for non-positioned files.

static int io61_fill(io61_file* f) {
    assert(f->tag == f->end_tag && f->pos_tag == f->end_tag);
    ssize_t nr;
    while (true) {
        nr = read(f->fd, f->cbuf, f->cbufsz);
        if (nr >= 0) {
            break;
        } else if (errno != EINTR && errno != EAGAIN) {
            return -1;
        }
    }
    f->end_tag += nr;
    return 0;
}


// io61_flush_*(f)
//    Helper functions for io61_flush.

static int io61_flush_dirty(io61_file* f) {
    // Called when `f`’s cache is dirty and not positioned.
    // Uses `write`; assumes that the initial file position equals `f->tag`.
    off_t flush_tag = f->tag;
    while (flush_tag != f->end_tag) {
        ssize_t nw = write(f->fd, &f->cbuf[flush_tag - f->tag],
                           f->end_tag - flush_tag);
        if (nw >= 0) {
            flush_tag += nw;
        } else if (errno != EINTR && errno != EINVAL) {
            return -1;
        }
    }
    f->dirty = false;
    f->tag = f->pos_tag = f->end_tag;
    return 0;
}

static int io61_flush_dirty_positioned(io61_file* f) {
    // Called when `f`’s cache is dirty and positioned.
    // Uses `pwrite`; does not change file position.
    off_t flush_tag = f->tag;
    while (flush_tag != f->end_tag) {
        ssize_t nw = pwrite(f->fd, &f->cbuf[flush_tag - f->tag],
                            f->end_tag - flush_tag, flush_tag);
        if (nw >= 0) {
            flush_tag += nw;
        } else if (errno != EINTR && errno != EINVAL) {
            return -1;
        }
    }
    f->dirty = false;
    return 0;
}

static int io61_flush_clean(io61_file* f) {

    // If we're in positioned mode, convert back to non-positioned mode.
    // This will do the right lseek and reset tag/pos_tag/end_tag.
    if (f->positioned) {
        return io61_maybe_unposition(f);
    }

    // Called when `f`’s cache is clean.
    if (f->seekable) {
        if (lseek(f->fd, f->pos_tag, SEEK_SET) == -1) {
            return -1;
        }
        f->tag = f->end_tag = f->pos_tag;
    }
    return 0;
}



// POSITIONED I/O FUNCTIONS

// io61_pread(f, buf, sz, off)
//    Read up to `sz` bytes from `f` into `buf`, starting at offset `off`.
//    Returns the number of characters read or -1 on error.
//
//    This function can only be called when `f` was opened in read/write
//    more (O_RDWR).

static int io61_pfill(io61_file* f, off_t off);

ssize_t io61_pread(io61_file* f, unsigned char* buf, size_t sz,
                   off_t off) {
    
    // Acquire coarse-grained range lock
    io61_lock(f, off, sz, LOCK_EX);

     // positioned I/O requires read/write
     if (f->mode != O_RDWR) {
        errno = EINVAL;
        return -1;
    }

    if (!f->positioned || off < f->tag || off >= f->end_tag) {
        if (io61_pfill(f, off) == -1) {
            return -1;
        }
    }
    // compute number of bytes left in the cache from offset off
    size_t nleft = f->end_tag - off;
    // if nothing left to read
    if (nleft == 0) {
        // then EOF and return 0
        return 0;
    }
    
    // copy as much as possible from cache to buf
    size_t ncopy = std::min(sz, nleft);
    memcpy(buf, &f->cbuf[off - f->tag], ncopy);

    // Release range lock
    io61_unlock(f, off, sz);

    return ncopy;
}


// io61_pwrite(f, buf, sz, off)
//    Write up to `sz` bytes from `buf` into `f`, starting at offset `off`.
//    Returns the number of characters written or -1 on error.
//
//    This function can only be called when `f` was opened in read/write
//    more (O_RDWR).

ssize_t io61_pwrite(io61_file* f, const unsigned char* buf, size_t sz,
                    off_t off) {
    
    // Acquire coarse-grained range lock
    io61_lock(f, off, sz, LOCK_EX);

    if (!f->positioned || off < f->tag || off >= f->end_tag) {
        if (io61_pfill(f, off) == -1) {
            return -1;
        }
    }
    size_t nleft = f->end_tag - off;
    size_t ncopy = std::min(sz, nleft);
    memcpy(&f->cbuf[off - f->tag], buf, ncopy);
    f->dirty = true;

    // Release range lock
    io61_unlock(f, off, sz);
    return ncopy;
}


// io61_pfill(f, off)
//    Fill the single-slot cache with data including offset `off`.
//    The handout code rounds `off` down to a multiple of 8192.

static int io61_pfill(io61_file* f, off_t off) {
    assert(f->mode == O_RDWR);
    if (f->dirty && io61_flush(f) == -1) {
        return -1;
    }

    off = off - (off % 8192);
    ssize_t nr = pread(f->fd, f->cbuf, f->cbufsz, off);
    if (nr == -1) {
        return -1;
    }
    f->tag = off;
    f->end_tag = off + nr;
    f->positioned = true;
    return 0;
}



// FILE LOCKING FUNCTIONS

// io61_try_lock(f, off, len, locktype)
//    Attempts to acquire a lock on offsets `[off, off + len)` in file `f`.
//    `locktype` must be `LOCK_EX`, which requests an exclusive lock. At most
//    one exclusive lock can be held on any offset of file data at a time.
//    Returns 0 if the lock was acquired and -1 if it was not.
//
//    This function does not block; if the lock cannot be required, it returns
//    -1 right away.
//
//    A thread may hold multiple locks on the same file, but no thread may
//    attempt to lock overlapping offset ranges. Threads in our test programs
//    always lock nonoverlapping ranges.

int io61_try_lock(io61_file* f, off_t off, off_t len, int locktype) {
    // off and len holds the range to be locked
    // so checking if the range is valid
    
    // if the length is 0
    if (len == 0) {
        // nothing to lock, so return 0
        return 0;
    }

    // LOCK_EX means exclusive lock (no other thread can hold the lock at the same time)
    assert(locktype == LOCK_EX);

    // phase 2, protect access to locks_held vector using recursive mutex
    std::unique_lock<std::mutex> guard (f->m);
    if (may_overlap_with_other_lock(f, off, len)) {
        errno = EAGAIN;
        return -1;
    }

    int rstart = file_region(off);
    int rend   = file_region(off + len - 1);

    for (int ri = rstart; ri <= rend; ++ri) {
        ++f->reg[ri].locked;
        f->reg[ri].owner = std::this_thread::get_id();
    }

    return 0;
}

// io61_lock(f, off, len, locktype)
//    Acquire a lock on offsets `[off, off + len)` in file `f`.
//    `locktype` must be `LOCK_EX`, which requests an exclusive lock; at most
//    one exclusive lock can be held on any offset of file data at a time.
//
//    Returns 0 if the lock was acquired and -1 on error. Blocks until
//    the lock can be acquired; the -1 return value is reserved for true
//    error conditions, such as EDEADLK (a deadlock was detected). Note that
//    your code need not detect deadlock.

int io61_lock(io61_file* f, off_t off, off_t len, int locktype) {
    if (len == 0) {
        return 0;
    }
    assert(locktype == LOCK_EX);

    std::unique_lock<std::mutex> guard(f->m);

    while (may_overlap_with_other_lock(f, off, len)) {
        f->cv.wait(guard);
    }

    int rstart = file_region(off);
    int rend   = file_region(off + len - 1);

    for (int ri = rstart; ri <= rend; ++ri) {
        ++f->reg[ri].locked;
        f->reg[ri].owner = std::this_thread::get_id();
    }

    return 0;
}


// io61_unlock(f, off, len)
//    Release the lock on offsets `[off, off + len)` in file `f`.
//    Returns 0 on success and -1 on error. The calling thread must have
//    previously acquired a lock on that offset range.

int io61_unlock(io61_file* f, off_t off, off_t len) {
      // if the length is 0
      if (len == 0) {
        // nothing to unlock, so return 0
        return 0;
    }

    std::unique_lock<std::mutex> guard(f->m);

    int rstart = file_region(off);
    int rend   = file_region(off + len - 1);

    for (int ri = rstart; ri <= rend; ++ri) {
        --f->reg[ri].locked;
    }

    f->cv.notify_all();
    return 0;
}



// HELPER FUNCTIONS
// You shouldn't need to change these functions.

// io61_open_check(filename, mode)
//    Opens the file corresponding to `filename` and returns its io61_file.
//    If `!filename`, returns either the standard input or the
//    standard output, depending on `mode`. Exits with an error message if
//    `filename != nullptr` and the named file cannot be opened.

io61_file* io61_open_check(const char* filename, int mode) {
    int fd;
    if (filename) {
        fd = open(filename, mode, 0666);
    } else if ((mode & O_ACCMODE) == O_RDONLY) {
        fd = STDIN_FILENO;
    } else {
        fd = STDOUT_FILENO;
    }
    if (fd < 0) {
        fprintf(stderr, "%s: %s\n", filename, strerror(errno));
        exit(1);
    }
    return io61_fdopen(fd, mode & O_ACCMODE);
}


// io61_fileno(f)
//    Returns the file descriptor associated with `f`.

int io61_fileno(io61_file* f) {
    return f->fd;
}


// io61_filesize(f)
//    Returns the size of `f` in bytes. Returns -1 if `f` does not have a
//    well-defined size (for instance, if it is a pipe).

off_t io61_filesize(io61_file* f) {
    struct stat s;
    int r = fstat(f->fd, &s);
    if (r >= 0 && S_ISREG(s.st_mode)) {
        return s.st_size;
    } else {
        return -1;
    }
}
