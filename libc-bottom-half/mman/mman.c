// Userspace emulation of mmap, munmap and msync for WASIX.
//
// There is no MMU underneath us, so mappings are carved out of linear
// memory. All mappings are served in whole wasm pages (64 KiB, which is
// also the page size libc reports via sysconf(_SC_PAGESIZE)), drawn
// directly from sbrk (memory.grow) and tracked in a side table. This
// gives page-aligned return values, partial munmap with POSIX
// semantics, and — crucially — lazy physical commit for anonymous
// mappings: pages freshly grown from linear memory are guaranteed
// zero-filled by the wasm spec, so they are handed out without being
// touched, and the host only commits them when the application first
// writes. Only pages recycled from a previous munmap are zeroed
// explicitly.
//
// Remaining limits of userspace emulation, documented rather than
// hidden: memory protections are not enforced (PROT_NONE and PROT_EXEC
// are rejected), MAP_FIXED is not supported, file-backed mappings are
// eager copies (no CoW; changes to the underlying file after mmap are
// not reflected), and MAP_SHARED writeback to the file happens on
// msync and munmap rather than continuously. Unmapped pages are
// recycled for future mappings but never returned to the OS, which is
// the same guarantee malloc provides (wasm linear memory cannot
// shrink).

#ifdef __wasilibc_unmodified_upstream
#define _WASI_EMULATED_MMAN
#else
#define _WASI_EMULATED_MMAN 1
#endif
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include <__macro_PAGESIZE.h>

#define PAGE ((size_t)PAGESIZE)

// A shared-writable file mapping keeps a duplicated file descriptor for
// writeback. Fragments produced by partial munmap share it, refcounted.
struct file_ref {
    int fd;
    size_t refs;
};

enum {
    R_FREE, // pages in the pool, available for reuse
    R_ANON, // live anonymous mapping
    R_FILE, // live file-backed mapping
};

struct region {
    uintptr_t base; // page-aligned
    size_t len;     // whole pages, > 0
    int kind;
    // R_FREE only: contents may be non-zero (recycled), so they must be
    // zeroed when handed out again. Pages fresh from sbrk are never
    // entered into the table as free, so this is always true today; the
    // flag exists so future changes (e.g. pre-grown pools) stay honest.
    bool dirty;
    int prot;
    int flags;
    struct file_ref *fref; // R_FILE with MAP_SHARED|PROT_WRITE, else NULL
    off_t off;             // R_FILE: file offset corresponding to base
    size_t data_len;       // R_FILE: bytes actually backed by the file
};

static pthread_mutex_t mman_lock = PTHREAD_MUTEX_INITIALIZER;
static struct region *regs;
static size_t nregs;
static size_t capregs;

static size_t page_ceil(size_t n) {
    return (n + PAGE - 1) & ~(PAGE - 1);
}

// Index of the first region whose end is above addr, i.e. the first
// region that can overlap [addr, ...). nregs if none.
static size_t first_overlap(uintptr_t addr) {
    size_t lo = 0, hi = nregs;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (regs[mid].base + regs[mid].len <= addr)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo;
}

// Make room for `extra` more regions. Called before any mutation so
// that the mutation itself cannot fail.
static int reserve(size_t extra) {
    if (nregs + extra <= capregs)
        return 0;
    size_t newcap = capregs ? capregs * 2 : 16;
    while (newcap < nregs + extra)
        newcap *= 2;
    struct region *newregs = realloc(regs, newcap * sizeof *regs);
    if (!newregs)
        return -1;
    regs = newregs;
    capregs = newcap;
    return 0;
}

// Callers must have reserved capacity.
static void insert_at(size_t i, struct region r) {
    memmove(&regs[i + 1], &regs[i], (nregs - i) * sizeof *regs);
    regs[i] = r;
    nregs++;
}

static void remove_at(size_t i) {
    memmove(&regs[i], &regs[i + 1], (nregs - i - 1) * sizeof *regs);
    nregs--;
}

static void fref_release(struct file_ref *fref) {
    if (fref && --fref->refs == 0) {
        close(fref->fd);
        free(fref);
    }
}

// Write the overlap of [start, end) with region r back to its file,
// clipped to the bytes the file actually backs. Returns 0 on success.
static int writeback(const struct region *r, uintptr_t start, uintptr_t end) {
    uintptr_t backed_end = r->base + r->data_len;
    if (end > backed_end)
        end = backed_end;
    if (start >= end)
        return 0;

    const char *body = (const char *)start;
    size_t remaining = end - start;
    off_t off = r->off + (off_t)(start - r->base);
    while (remaining > 0) {
        ssize_t n = pwrite(r->fref->fd, body, remaining, off);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        remaining -= (size_t)n;
        off += n;
        body += n;
    }
    return 0;
}

// Merge adjacent free regions in the vicinity of [start, end).
static void coalesce_around(uintptr_t start, uintptr_t end) {
    size_t i = first_overlap(start);
    if (i > 0)
        i--;
    while (i + 1 < nregs && regs[i].base <= end) {
        if (regs[i].kind == R_FREE && regs[i + 1].kind == R_FREE &&
            regs[i].base + regs[i].len == regs[i + 1].base) {
            regs[i].len += regs[i + 1].len;
            regs[i].dirty = regs[i].dirty || regs[i + 1].dirty;
            remove_at(i + 1);
        } else {
            i++;
        }
    }
}

// Take `len` bytes (page multiple) of pool pages: first-fit over free
// regions, falling back to growing linear memory. On success the new
// region is in the table at *idx_out with kind/prot/flags set to the
// given values and its contents zero-filled (either fresh pages, or
// recycled pages that get memset here). Returns -1 with errno set on
// failure. Requires 2 reserved slots.
static int acquire_pages(size_t len, int kind, int prot, int flags,
                         size_t *idx_out) {
    for (size_t i = 0; i < nregs; i++) {
        if (regs[i].kind != R_FREE || regs[i].len < len)
            continue;
        bool dirty = regs[i].dirty;
        if (regs[i].len > len) {
            // Take from the front, keep the rest free.
            struct region taken = regs[i];
            taken.len = len;
            regs[i].base += len;
            regs[i].len -= len;
            insert_at(i, taken);
        }
        regs[i].kind = kind;
        regs[i].dirty = false;
        regs[i].prot = prot;
        regs[i].flags = flags;
        regs[i].fref = NULL;
        regs[i].off = 0;
        regs[i].data_len = 0;
        if (dirty)
            memset((void *)regs[i].base, 0, len);
        *idx_out = i;
        return 0;
    }

    // No free run fits; grow linear memory. sbrk only accepts
    // page-multiple increments and returns the old memory size, which
    // is always page-aligned; the new pages are zero by the wasm spec.
    void *grown = sbrk((intptr_t)len);
    if (grown == (void *)-1) {
        errno = ENOMEM;
        return -1;
    }
    struct region fresh = {
        .base = (uintptr_t)grown,
        .len = len,
        .kind = kind,
        .dirty = false,
        .prot = prot,
        .flags = flags,
        .fref = NULL,
        .off = 0,
        .data_len = 0,
    };
    size_t i = first_overlap(fresh.base);
    insert_at(i, fresh);
    *idx_out = i;
    return 0;
}

// Unmap [start, end) (page-aligned). Live pages become free (dirty)
// pool pages; gaps and already-free pages in the range are skipped,
// matching POSIX/Linux munmap semantics. Shared-writable file pages are
// written back best-effort before being freed. Caller holds the lock
// and has reserved 2 slots.
static void unmap_range(uintptr_t start, uintptr_t end) {
    size_t i = first_overlap(start);
    while (i < nregs && regs[i].base < end) {
        struct region *r = &regs[i];
        if (r->kind == R_FREE) {
            i++;
            continue;
        }
        uintptr_t rbase = r->base;
        uintptr_t rend = r->base + r->len;
        uintptr_t ostart = rbase > start ? rbase : start;
        uintptr_t oend = rend < end ? rend : end;

        if (r->kind == R_FILE && r->fref && (r->prot & PROT_WRITE) != 0) {
            // Best-effort, like the kernel's asynchronous writeback;
            // msync is the API that reports errors.
            (void)writeback(r, ostart, oend);
        }

        if (ostart == rbase && oend == rend) {
            // Whole region unmapped.
            if (r->kind == R_FILE)
                fref_release(r->fref);
            r->kind = R_FREE;
            r->dirty = true;
            r->fref = NULL;
            i++;
        } else if (ostart == rbase) {
            // Head unmapped; tail survives.
            struct region tail = *r;
            tail.base = oend;
            tail.len = rend - oend;
            if (tail.kind == R_FILE) {
                size_t cut = oend - rbase;
                tail.off += (off_t)cut;
                tail.data_len = tail.data_len > cut ? tail.data_len - cut : 0;
            }
            r->len = oend - rbase;
            r->kind = R_FREE;
            r->dirty = true;
            r->fref = NULL;
            insert_at(i + 1, tail);
            i += 2;
        } else if (oend == rend) {
            // Tail unmapped; head survives.
            struct region freed = {
                .base = ostart,
                .len = rend - ostart,
                .kind = R_FREE,
                .dirty = true,
            };
            r->len = ostart - rbase;
            if (r->kind == R_FILE && r->data_len > r->len)
                r->data_len = r->len;
            insert_at(i + 1, freed);
            i += 2;
        } else {
            // Middle unmapped; head and tail survive.
            struct region freed = {
                .base = ostart,
                .len = oend - ostart,
                .kind = R_FREE,
                .dirty = true,
            };
            struct region tail = *r;
            tail.base = oend;
            tail.len = rend - oend;
            if (tail.kind == R_FILE) {
                size_t cut = oend - rbase;
                tail.off += (off_t)cut;
                tail.data_len = tail.data_len > cut ? tail.data_len - cut : 0;
                if (tail.fref)
                    tail.fref->refs++;
            }
            r->len = ostart - rbase;
            if (r->kind == R_FILE && r->data_len > r->len)
                r->data_len = r->len;
            insert_at(i + 1, freed);
            insert_at(i + 2, tail);
            i += 3;
        }
    }
    coalesce_around(start, end);
}

void *mmap(void *addr, size_t length, int prot, int flags,
           int fd, off_t offset) {
    (void)addr; // hint, unused

    // Check for unsupported flags.
    if ((flags & (MAP_PRIVATE | MAP_SHARED)) == 0 ||
        (flags & MAP_FIXED) != 0 ||
#ifdef MAP_SHARED_VALIDATE
        (flags & MAP_SHARED_VALIDATE) == MAP_SHARED_VALIDATE ||
#endif
#ifdef MAP_GROWSDOWN
        (flags & MAP_GROWSDOWN) != 0 ||
#endif
#ifdef MAP_HUGETLB
        (flags & MAP_HUGETLB) != 0 ||
#endif
#ifdef MAP_FIXED_NOREPLACE
        (flags & MAP_FIXED_NOREPLACE) != 0 ||
#endif
        0)
    {
        errno = EINVAL;
        return MAP_FAILED;
    }
    // MAP_NORESERVE is accepted as a no-op: commit is lazy anyway.

    // Check for unsupported protection requests. Protections cannot be
    // enforced without an MMU; PROT_NONE and PROT_EXEC are rejected
    // loudly rather than silently not meaning what the caller intended.
    if (prot == PROT_NONE ||
#ifdef PROT_EXEC
        (prot & PROT_EXEC) != 0 ||
#endif
        0)
    {
        errno = EINVAL;
        return MAP_FAILED;
    }

    if (length == 0) {
        errno = EINVAL;
        return MAP_FAILED;
    }
    if (length >= PTRDIFF_MAX) {
        errno = ENOMEM;
        return MAP_FAILED;
    }

    bool anon = (flags & MAP_ANON) != 0;
    if (!anon) {
        if (fd < 0) {
            errno = EBADF;
            return MAP_FAILED;
        }
        if (offset < 0 || (offset & (off_t)(PAGE - 1)) != 0) {
            errno = EINVAL;
            return MAP_FAILED;
        }
    }

    size_t len = page_ceil(length);
    if (len < length) { // overflow
        errno = ENOMEM;
        return MAP_FAILED;
    }

    pthread_mutex_lock(&mman_lock);

    size_t idx;
    if (reserve(2) != 0 ||
        acquire_pages(len, anon ? R_ANON : R_FILE, prot, flags, &idx) != 0) {
        pthread_mutex_unlock(&mman_lock);
        errno = ENOMEM;
        return MAP_FAILED;
    }
    uintptr_t base = regs[idx].base;

    if (!anon) {
        // Populate whole pages from the file, like a real mapping
        // would; reads stop naturally at EOF and the remainder is
        // already zero. Changes to the file after this point are not
        // reflected in the mapping (no CoW / no shared page cache).
        char *body = (char *)base;
        size_t remaining = len;
        off_t off = offset;
        size_t total = 0;
        while (remaining > 0) {
            ssize_t n = pread(fd, body, remaining, off);
            if (n < 0) {
                if (errno == EINTR)
                    continue;
                int saved = errno;
                unmap_range(base, base + len);
                pthread_mutex_unlock(&mman_lock);
                errno = saved;
                return MAP_FAILED;
            }
            if (n == 0)
                break;
            remaining -= (size_t)n;
            off += n;
            body += n;
            total += (size_t)n;
        }
        regs[idx].off = offset;
        regs[idx].data_len = total;

        if ((flags & MAP_SHARED) != 0 && (prot & PROT_WRITE) != 0) {
            struct file_ref *fref = malloc(sizeof *fref);
            int newfd = fref ? dup(fd) : -1;
            if (!fref || newfd < 0) {
                int saved = fref ? errno : ENOMEM;
                free(fref);
                unmap_range(base, base + len);
                pthread_mutex_unlock(&mman_lock);
                errno = saved;
                return MAP_FAILED;
            }
            fref->fd = newfd;
            fref->refs = 1;
            regs[idx].fref = fref;
        }
    }

    pthread_mutex_unlock(&mman_lock);
    return (void *)base;
}

int munmap(void *addr, size_t length) {
    uintptr_t start = (uintptr_t)addr;
    if ((start & (PAGE - 1)) != 0 || length == 0) {
        errno = EINVAL;
        return -1;
    }
    size_t len = page_ceil(length);
    if (len < length || start + len < start) {
        errno = EINVAL;
        return -1;
    }

    pthread_mutex_lock(&mman_lock);
    if (reserve(2) != 0) {
        pthread_mutex_unlock(&mman_lock);
        errno = ENOMEM;
        return -1;
    }
    unmap_range(start, start + len);
    pthread_mutex_unlock(&mman_lock);
    return 0;
}

int msync(void *addr, size_t length, int flags) {
    uintptr_t start = (uintptr_t)addr;
    if ((start & (PAGE - 1)) != 0 ||
        ((flags & MS_ASYNC) != 0 && (flags & MS_SYNC) != 0) ||
        (flags & ~(MS_ASYNC | MS_SYNC | MS_INVALIDATE)) != 0) {
        errno = EINVAL;
        return -1;
    }
    if (length == 0)
        return 0;
    size_t len = page_ceil(length);
    if (len < length || start + len < start) {
        errno = ENOMEM;
        return -1;
    }
    uintptr_t end = start + len;

    pthread_mutex_lock(&mman_lock);

    // The whole range must be covered by live mappings.
    uintptr_t cursor = start;
    size_t i = first_overlap(start);
    size_t begin = i;
    while (cursor < end) {
        if (i >= nregs || regs[i].kind == R_FREE || regs[i].base > cursor) {
            pthread_mutex_unlock(&mman_lock);
            errno = ENOMEM;
            return -1;
        }
        cursor = regs[i].base + regs[i].len;
        i++;
    }

    int result = 0;
    for (i = begin; i < nregs && regs[i].base < end; i++) {
        struct region *r = &regs[i];
        if (r->kind != R_FILE || !r->fref || (r->prot & PROT_WRITE) == 0)
            continue;
        uintptr_t ostart = r->base > start ? r->base : start;
        uintptr_t oend = r->base + r->len < end ? r->base + r->len : end;
        if (writeback(r, ostart, oend) != 0)
            result = -1; // errno set by pwrite
    }

    pthread_mutex_unlock(&mman_lock);
    return result;
}
