#include "io61.hh"
#include <sys/types.h>
#include <sys/stat.h>
#include <climits>
#include <cerrno>
#include <assert.h>
#include <cstring> // for memcpy
#include <unistd.h> // read, write, lseek, close
#include <fcntl.h>  
#include <sys/stat.h>


// io61.cc !

// io61_file
// data structure for io61 file wrappers
struct io61_file {
    int fd = -1; // initally not open
    int mode; // can only be O_RDONLY or O_WRONLY

    // creating single slot read cache (phase 2)
    enum { bufsz = 8192 }; // size of read buffer

    // read buffer 
    unsigned char cbuf[bufsz]; // holds thea actual cached data
    off_t tag; // file offset of cbuf[0] (start of cached window)
    off_t end_tag; // end of valid Sdata in cbuf

    // write buffer
    unsigned char wbuf[bufsz];
    off_t wtag;
    off_t wend_tag;


    off_t pos; // current position in file
    bool write_mode = false; // to track when file is in write mode, initally false bc opened in read mode
    bool seekable = true; // initially assume file is seekable

    // character devices are not seekable, so this helps optimize behavior
    bool is_chardev = false; // true if fd is a character device (ex reading user input from terminal)

    // how far we've read so far
    off_t pipe_highwater = 0;
};

// before caching file data, need to check if file is seekable
// also adding logic to separate out character devices
static inline void probe_fd(io61_file* f) { // declaring f as pointer to io61_file struct
    struct stat st; // holds fields that describe file attributes
    // using fstat to get metadata about file associated with fd
    if (fstat(f->fd, &st) < 0) { // if file has been closed or an error has occured
        f->seekable = false; // not seekable
        f->is_chardev = false; // not a char device
        return; // return updated fields in struct
    }

    // S_ISCHR checks if file is a character device
    // if mode indicates character device
    f->is_chardev = S_ISCHR(st.st_mode); // set is_chardev using . operator bc st is direct instance of struct stat
    // S_ISREG checks if file is a regular file
    // S_ISBLK checks if file is a block device (can support random access through lseek)
    // if mode indicates file is a regular file OR block device (only file types that support lseek)
    f->seekable = S_ISREG(st.st_mode) || S_ISBLK(st.st_mode); // update seekable field in struct 
}

// function to check cache invariants (calling before & after caching)
static inline void cache_check(const io61_file* f) {
    // *f is the io61_file struct itself
    // f->tag gets the tag field in the struct pointed to by f
    // invariants are conditions that should always hold true !

    // ensure pos lies within cached range [tag, end_tag] before caching
    assert(f->tag <= f->pos && f->pos <= f->end_tag); 

    // ensure that cached data fits within buffer capacity
    assert((size_t)(f->end_tag - f->tag) <= sizeof f->cbuf); 
}

// filling cache with file data
static ssize_t cache_fill(io61_file* f) {
    // ensure the cache is empty before filling cache
    if (f->pos < f->end_tag) {
        return -1; // -1 indicates error: cache not empty
    }

    // check invariants before filling cache
    cache_check(f);


    // setting tag, pos, end_tag to current position
    f->tag = f->pos;
    f->end_tag = f->pos;

     // only proceed with lseek if file is seekable
     if (f->seekable) {
        // position the file descriptor at current position
        off_t r = lseek(f->fd, f->pos, SEEK_SET);
        // if lseek fails or doesn't set to current position
        if (r == -1 && errno != ESPIPE) {
            return -1; // can't fill cache
        }   
    }

    // got holds number of bytes read into cache
    // 0 indicates no bytes read yet
    size_t got = 0;
    // keep reading until buffer is full
    while (got < io61_file::bufsz) {
        // initiate the read system call
        // r holds number of bytes read
        ssize_t r = ::read(f->fd, f->cbuf + got, io61_file::bufsz - got);
        // so as long as there were bytes read, update got
        if (r > 0) {
            got += (size_t) r;
        // if r is 0, reached EOF
        } else if (r == 0) { 
            break;
        // errno is set by read syscall to indicate error type
        // EINTR = read was interrupted by signal (ex CTRL-C in terminal)
        // if EAGAIN or EWOULDBLOCK, fd is in non-blocking mode & -1 is returned
        } else if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) { 
            continue; // try reading again so dont treat as error INITALLY 
        } else {  // permanent error
            if (got == 0) return -1;
            break; // accept partial fill
        }
    }

    // update end_tag to reflect amount of data read
    f->end_tag = f->tag + (off_t) got;

    cache_check(f); // checking invariants after filling cache

    // 0 → EOF, -1 → error, >0 → bytes available to read
    return (ssize_t) got;
}


// io61_fdopen(fd, mode)
//    Returns a new io61_file for file descriptor `fd`. `mode` is either
//    O_RDONLY for a read-only file or O_WRONLY for a write-only file.
//    You need not support read/write files.

io61_file* io61_fdopen(int fd, int mode) {
    // ensure fd is valid
    assert(fd >= 0);
    // allocate new io61_file struct
    io61_file* f = new io61_file; 
    // go to address stored in f & set it's fd field to fd passed in
    f->fd = fd;
    // go to address stored in f & set it's mode field to mode passed in
    f->mode = (mode & O_ACCMODE);

    // ensure mode is either O_RDONLY or O_WRONLY
    assert(f->mode == O_RDONLY || f->mode == O_WRONLY);

    // single-slot read cache initialization
    // set all to 0 bc at start, no bytes read yet (cache must be empty before filling)
    f->tag = 0;
    f->end_tag = 0;
    f->pos = 0;

    // phase 3: write mode state initalization
    // iinitially opened in read mode
    f->write_mode = false;
    
    // phase 5: implementing "lazy" seeks so need to check if fd is seekable
    // before setting seekable field in struct
    probe_fd(f);
    return f; 
}


// io61_readc(f)
//    Reads a single (unsigned) byte from `f` and returns it. Returns EOF,
//    which equals -1, on end of file or error.

int io61_readc(io61_file* f) {

    // if prev in writing mode
    if (f->write_mode) {
        // flush cache first (bc can't switch to read mode until writes are resolved)
        // if flush fails, return -1 to indicate error
        if (io61_flush(f) < 0) {
            return -1;
        }
        // flushing switches to read mode !
        // after flushing, start a fresh (empty) read window at current position
        f->tag = f->pos;
        f->end_tag = f->pos;
    }

    // check if cache needs to be refilled with file data before reading a byte
    // if current position is at or beyond end_tag, cache is empty
    if (f->pos >= f->end_tag) { 
        // therefore need to fill cache with file data
        ssize_t filled = cache_fill(f); 
        // filled can't be <= 0 bc need at least one byte to read
        // handle refill failures
        if (filled <= 0) {

        // retry only if temporary error
        if (filled < 0 &&
        (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) {

        // try clearing and refilling once
        f->tag = f->end_tag = f->pos;
        filled = cache_fill(f);

        if (filled <= 0) {
            return -1; // still nothing → give up
        }

        } else {
            // either EOF (filled == 0) or real error (filled < 0)
            return -1;
        }
        
        }
    }

    // extract byte
    unsigned char ch = f->cbuf[f->pos - f->tag];
    f->pos += 1;
    return (int) ch;

    // safety return (should never be hit)
    return -1;
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

ssize_t io61_read(io61_file* f, unsigned char* buf, size_t sz) { // sz = size to read
   // if prev in writing mode
   if (f->write_mode) {
        // flush cache first (bc can't switch to read mode until writes are resolved)
        // if flush fails, return -1 to indicate error
        if (io61_flush(f) < 0) {
            return -1;
    }
    // flushing switches to read mode !
    // after flushing, start a fresh (empty) read window at current position
    f->tag = f->pos;
    f->end_tag = f->pos;
    }

    size_t nread = 0; // initilally 0 bytes read
    while (nread < sz) { // while there are still bytes to read
        // check if cache needs to be refilled with file data before reading more bytes
        // if cache is empty
        if (f->pos >= f->end_tag) { 
            // allocate file data into cache
            ssize_t filled = cache_fill(f);
            // handle error or EOF during cache fill
            if (filled < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                    // Retry — do NOT return -1
                    continue;
                }
                return -1;
            }
            if (filled == 0) {
                // EOF
                return (ssize_t) nread;
            }
        }

        // copy from slot in one chunk
        // reduce number of memcpy calls for efficiency -> phase 2
        size_t cache_avail = (size_t)(f->end_tag - f->pos);
        size_t want = sz - nread;
        size_t take = cache_avail < want ? cache_avail : want;

        memcpy(buf + nread, f->cbuf + (f->pos - f->tag), take);
        f->pos += (off_t) take;
        nread += take;  
    }
    // at the end of system call reading, return number of bytes read
    return (ssize_t) nread; 
}

// io61_writec(f)
//    Write a single character `c` to `f` (converted to unsigned char).
//    Returns 0 on success and -1 on error.

int io61_writec(io61_file* f, int c) { // c is character to write
    // if prev in read mode, need to switch to write mode
    if (!f->write_mode) {

        // before switching, align kernel offset with pos using lseek
        // kernel offset may be out of sync with cbuf 
        off_t rr = lseek(f->fd, f->pos, SEEK_SET);
        // if lseek fails and isn't a non-seekable file
        if (rr == -1 && errno != ESPIPE) {
            return -1; // indicate error
        }
        // switch to write mode
        f->write_mode = true;
        // start of pending writes
        f->wtag = f->pos;
        // empty write buffer before caching 
        f->wend_tag = f->pos;       
    }

    // handle reverse / non-sequential writes before sequential writes
    // if pos is not at end_tag
    if (f->pos != f->wend_tag) {


        // FIRST flush previous window so bytes aren't dropped
        if (io61_flush(f) < 0) {
            // indicate error if flush fails
            return -1;
        }

        // THEN align kernel offset is seekable (meaning sequential writes)
        if (f->seekable) {
            // use lseek to align kernel offset with pos
            off_t rr = lseek(f->fd, f->pos, SEEK_SET);
            // if lseek doesn't set to current position
            if (rr == -1 && errno != ESPIPE) {
                // indicate error
                return -1;
            }
        }
        // update buffer state after handling reverse write
        f->wtag = f->pos;
        f->wend_tag = f->pos;
        f->write_mode = true;
    }

    //  if buffer is full
    if ((size_t)(f->wend_tag - f->wtag) == io61_file::bufsz) {

        if (io61_flush(f) < 0) {
            return -1;
        }

        // re-enter write mode with now empty buffer
        f->write_mode = true;
        f->wtag = f->pos;
        f->wend_tag = f->pos;
    }

    // buffer the 1 byte to write
    f->wbuf[f->wend_tag - f->wtag] = (unsigned char) c;
    f->wend_tag += 1;
    f->pos += 1;
    return 0; // indicate success
}

// io61_write(f, buf, sz)
//    Writes `sz` characters from `buf` to `f`. Returns `sz` on success.
//    Can write fewer than `sz` characters when there is an error, such as
//    a drive running out of space. In this case io61_write returns the
//    number of characters written, or -1 if no characters were written
//    before the error occurred.

ssize_t io61_write(io61_file* f, const unsigned char* buf, size_t sz) {
    // number of bytes written so far
    size_t nwritten = 0; 

    // if prev in read mode
    if (!f->write_mode) {
        // align kernel offset with pos using lseek
        off_t rr = lseek(f->fd, f->pos, SEEK_SET);
        // if lseek fails or doesn't set to current position
        if (rr == -1 && errno != ESPIPE) {
            // only treat as error if no bytes were written yet
            return nwritten > 0 ? (ssize_t) nwritten : -1;
        }

        // switch to write mode
        f->write_mode = true;
        // start of pending writes
        f->wtag = f->pos;  
        // empty write buffer before caching
        f->wend_tag = f->pos;
    }

    // handle reverse / non-sequential writes
    if (f->pos != f->wend_tag) {
        // flush first
        if (io61_flush(f) < 0) {
            // only treat as error if no bytes were written yet
            return nwritten > 0 ? (ssize_t) nwritten : -1;
        }

        // if can seek
        if (f->seekable) {
            // use lseek to align kernel offset with pos
            off_t rr = lseek(f->fd, f->pos, SEEK_SET);
            // if lseek doesn't set to current position or fails
            if (rr == -1 && errno != ESPIPE) {
                // only treat as error if no bytes were written yet
                return nwritten > 0 ? (ssize_t) nwritten : -1;
            }
        }

        // reset MUST happen outside of if seekable block
        f->wtag = f->pos;
        f->wend_tag = f->pos;
        f->write_mode = true;
    }

    // write data to the buffer
    while (nwritten < sz) {
        // used = number of bytes currently in buffer
        size_t used = (size_t)(f->wend_tag - f->wtag);
        // space = number of bytes available in buffer
        size_t space = io61_file::bufsz - used;

        // if no space left
        if (space == 0) { 
            // flush buffer to make space
            if (io61_flush(f) < 0) {
             // if some bytes were written before error
                if (nwritten > 0) {
                    // return the number of bytes written so far for caller to handle partial write
                    return (ssize_t)nwritten; 
                } else {
                    return -1; // indicate error
                }
            }   
            // reset buffer state after flush
            f->wtag = f->pos;
            f->wend_tag = f->pos;
            // after restting, no bytes are used
            used = 0;
            space = io61_file::bufsz;
        }

        // copy as much as possible into buffer
        size_t copy_sz = (space < (sz - nwritten) ? space : (sz - nwritten));
        memcpy(f->wbuf + used, buf + nwritten, copy_sz);

        // update buffer and file state after copying
        f->wend_tag += (off_t)copy_sz;
        f->pos += (off_t)copy_sz;
        nwritten += copy_sz;

        // if buffer is full after copying
        if ((size_t)(f->wend_tag - f->wtag) == io61_file::bufsz) {
            // flush buffer to file
            if (io61_flush(f) < 0) {
                return (ssize_t) nwritten;        
            }
        }
    }
    // return total number of bytes written
    return (ssize_t) nwritten;
}

// io61_close(f)
//    Closes the io61_file `f` and releases all its resources.

int io61_close(io61_file* f) {
    if (io61_flush(f) < 0) { /* keep going but report close() result */ }
    int r = close(f->fd);
    delete f;
    return r;
}


// io61_flush(f)
//    If `f` was opened write-only, `io61_flush(f)` forces a write of any
//    cached data written to `f`. Returns 0 on success; returns -1 if an error
//    is encountered before all cached data was written.
//
//    If `f` was opened read-only, `io61_flush(f)` returns 0. It may also
//    drop any data cached for reading.

int io61_flush(io61_file* f) {

    // if not in write mode, nothing to flush
    if (!f->write_mode) {
        // invalidate read buffer
        f->tag = f->pos;
        f->end_tag = f->pos;
        return 0;
    }

    // pending bytes to write from cbuf
    size_t pending = (size_t)(f->wend_tag - f->wtag);
    // off = number of bytes written so far
    size_t off = 0;  

    // if there’s anything to flush and the FD is seekable
    if (pending > 0 && f->seekable) {
        // align kernel offset with tag
        off_t rr = lseek(f->fd, f->wtag, SEEK_SET);
        // if lseek fails or doesn't set to tag position
        if (rr == -1) {
            return -1; // failed to flush
        }
    }

    // while there are still pending bytes to write
    while (off < pending) {
        // write pending bytes from cbuf to file
        ssize_t w = ::write(f->fd, f->wbuf + off, pending - off);
        // if some bytes were written
        if (w > 0) {
            // update off to reflect bytes written
            off += (size_t) w; 
        // if write was interrupted by signal or would block         
        } else if (w == 0 || errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
            continue; // try writing again
        } else { 
            // collect partial write info before returning error
            if (off > 0 && off < pending) {
                // shift remaining bytes to front of cbuf
                memmove(f->wbuf, f->wbuf + off, pending - off);
                f->wtag += (off_t) off; // update tag to reflect bytes written
            }
            return -1; // indicate error
        }
    }

    // success: all pending bytes written. advance tags to current pos
    // pos was already advanced during io61_write() bc copied in cbuf,
    // so after a full flush these should all match
    f->wtag = f->pos;
    f->wend_tag = f->pos;

    return 0; // can return success
}


// io61_seek(f, off)
//    Changes the file pointer for file `f` to `off` bytes into the file.
//    Returns 0 on success and -1 on failure.
//    off = desired offset to seek to

int io61_seek(io61_file* f, off_t off) {

    // only check cache if in read mode
    if (!f->write_mode) {
        cache_check(f);
    }

    // if in write mode, flush first
    if (f->write_mode && io61_flush(f) < 0) {
        return -1;
    }
    
    // if file is not seekable (character device, pipe, etc.)
    if (!f->seekable) {
        // if offset is same as current pos
        if (off <= f->pipe_highwater) {
            f->pos = off;
            f->tag = f->end_tag = off;
            return 0;
        }

      // If aiming forward past highwater: must read & discard ONLY the delta
      off_t remain = off - f->pipe_highwater;
      unsigned char tmp[4096];
  
      while (remain > 0) {
          size_t chunk = (remain < (off_t)sizeof tmp ? remain : (off_t)sizeof tmp);
          ssize_t nr = read(f->fd, tmp, chunk);
          if (nr <= 0) return -1;
          f->pipe_highwater += nr;
          remain -= nr;
      }
  
      f->pos = off;
      f->tag = f->end_tag = off;
      return 0;
    }
    f->pos = off;

    // Invalidate cache if target offset not in current window
    if (!(f->tag <= off && off < f->end_tag)) {
        f->tag = f->end_tag = off;
    }

    if (!f->write_mode) {
        cache_check(f);
    }

    return 0;
}

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
    if (r < 0 || !S_ISREG(s.st_mode)) {
        return -1;
    }
    return s.st_size;
}
