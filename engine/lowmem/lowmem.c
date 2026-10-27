/*
 * lowmem.c - Keep every heap allocation of the BennuGD runtime below 2 GB.
 *
 * BennuGD 1.x stores raw C pointers inside 32-bit interpreter slots
 * (int32 stack, POINTER variables, sound/music handles...). On 64-bit
 * targets (arm64 Android) a normal heap returns addresses far above 4 GB,
 * which get truncated. This allocator reserves address space in the range
 * [0x10000000, 0x7FFFFFFF] and serves malloc/calloc/realloc/free from it
 * through TLSF, so every pointer fits in a signed 32-bit int.
 *
 * It is hooked in with -Wl,--wrap=malloc,... for the engine objects and
 * with SDL_SetMemoryFunctions for SDL / SDL_mixer.
 */

#define _GNU_SOURCE
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/mman.h>
#include <malloc.h>

#include "tlsf.h"

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif
#ifndef MAP_NORESERVE
#define MAP_NORESERVE 0x4000
#endif

#define LOW_START   ((uintptr_t)0x10000000u)   /* 256 MB */
#define LOW_END     ((uintptr_t)0x7FFF0000u)   /* just under 2 GB */
#define POOL_SIZE   ((size_t)128u << 20)       /* 128 MB per pool */
#define MAX_POOLS   32

extern void *__real_malloc(size_t);
extern void  __real_free(void *);
extern void *__real_calloc(size_t, size_t);
extern void *__real_realloc(void *, size_t);

static pthread_mutex_t lm_lock = PTHREAD_MUTEX_INITIALIZER;
static tlsf_t   lm_tlsf;
static uintptr_t lm_lo[MAX_POOLS], lm_hi[MAX_POOLS];
static int      lm_npools;
static uintptr_t lm_next = LOW_START;
static int      lm_failed;

static void *lm_map(size_t size)
{
    while (lm_next + size <= LOW_END) {
        void *hint = (void *)lm_next;
        void *p = mmap(hint, size, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE, -1, 0);
        if (p == MAP_FAILED) {
            /* Old kernels: retry as a plain hint and check where it landed */
            p = mmap(hint, size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
        }
        if (p != MAP_FAILED) {
            uintptr_t a = (uintptr_t)p;
            if (a >= LOW_START && a + size <= LOW_END) {
                lm_next = a + size;
                return p;
            }
            munmap(p, size);
        }
        lm_next += (size_t)16u << 20; /* busy area: skip 16 MB and retry */
    }
    return NULL;
}

/* Must be called with the lock held */
static int lm_grow(size_t need)
{
    size_t size = POOL_SIZE;
    void *mem;
    if (lm_npools >= MAX_POOLS) return 0;
    need += tlsf_pool_overhead() + 4096;
    if (size < need) size = (need + 0xFFFFF) & ~(size_t)0xFFFFF;
    mem = lm_map(size);
    if (!mem) return 0;
    if (!lm_tlsf) {
        size_t ctl = (tlsf_size() + 4095) & ~(size_t)4095;
        lm_tlsf = tlsf_create(mem);
        mem = (char *)mem + ctl;
        size -= ctl;
        lm_lo[lm_npools] = (uintptr_t)mem - ctl;
    } else {
        lm_lo[lm_npools] = (uintptr_t)mem;
    }
    lm_hi[lm_npools] = (uintptr_t)mem + size;
    lm_npools++;
    fprintf(stderr, "lowmem: pool %d at %p (%zu MB)\n", lm_npools, mem, size >> 20);
    tlsf_add_pool(lm_tlsf, mem, size);
    return 1;
}

static int lm_owns(const void *p)
{
    uintptr_t a = (uintptr_t)p;
    int i;
    if (a < LOW_START || a >= LOW_END) return 0;
    for (i = 0; i < lm_npools; i++)
        if (a >= lm_lo[i] && a < lm_hi[i]) return 1;
    return 0;
}

static void *lm_alloc_locked(size_t size, size_t align)
{
    void *p = NULL;
    if (lm_failed) return NULL;
    if (!lm_tlsf && !lm_grow(size)) { lm_failed = 1; return NULL; }
    for (;;) {
        p = align > 8 ? tlsf_memalign(lm_tlsf, align, size ? size : 1)
                      : tlsf_malloc(lm_tlsf, size ? size : 1);
        if (p) break;
        if (!lm_grow(size + align)) break;
    }
    return p;
}

void *lowmem_malloc(size_t size)
{
    void *p;
    pthread_mutex_lock(&lm_lock);
    p = lm_alloc_locked(size, 0);
    pthread_mutex_unlock(&lm_lock);
    /* Hand out zeroed memory: the original engine (and SDL users of it) sometimes
       relies on fresh heap memory being zero, as it usually is on desktop systems. */
    if (p) memset(p, 0, size);
    if (!p) {
        static int warned;
        if (!warned) { warned = 1; fprintf(stderr, "lowmem: low arena exhausted, falling back to system heap\n"); }
        p = __real_malloc(size);
    }
    return p;
}

void lowmem_free(void *p)
{
    if (!p) return;
    pthread_mutex_lock(&lm_lock);
    if (lm_owns(p)) {
        tlsf_free(lm_tlsf, p);
        pthread_mutex_unlock(&lm_lock);
        return;
    }
    pthread_mutex_unlock(&lm_lock);
    __real_free(p);
}

void *lowmem_calloc(size_t n, size_t sz)
{
    void *p;
    size_t total;
    if (sz && n > (size_t)-1 / sz) return NULL;
    total = n * sz;
    p = lowmem_malloc(total);
    if (p) memset(p, 0, total);
    return p;
}

void *lowmem_realloc(void *old, size_t size)
{
    void *p;
    size_t oldsize;
    if (!old) return lowmem_malloc(size);
    if (!size) { lowmem_free(old); return NULL; }

    pthread_mutex_lock(&lm_lock);
    if (lm_owns(old)) {
        p = tlsf_realloc(lm_tlsf, old, size);
        if (!p) {
            oldsize = tlsf_block_size(old);
            p = lm_alloc_locked(size, 0);
            if (p) {
                memcpy(p, old, oldsize < size ? oldsize : size);
                tlsf_free(lm_tlsf, old);
            }
        }
        pthread_mutex_unlock(&lm_lock);
        return p;
    }
    pthread_mutex_unlock(&lm_lock);

    /* Block from the system heap (allocated before we took over, or by libc) */
    oldsize = malloc_usable_size(old);
    p = lowmem_malloc(size);
    if (p) {
        memcpy(p, old, oldsize < size ? oldsize : size);
        __real_free(old);
    }
    return p;
}

/* ---- link-time wrappers (-Wl,--wrap=...) ---- */
void *__wrap_malloc(size_t s)              { return lowmem_malloc(s); }
void  __wrap_free(void *p)                 { lowmem_free(p); }
void *__wrap_calloc(size_t n, size_t s)    { return lowmem_calloc(n, s); }
void *__wrap_realloc(void *p, size_t s)    { return lowmem_realloc(p, s); }

char *__wrap_strdup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *d = lowmem_malloc(n);
    if (d) memcpy(d, s, n);
    return d;
}

char *__wrap_strndup(const char *s, size_t max)
{
    size_t n = strnlen(s, max);
    char *d = lowmem_malloc(n + 1);
    if (d) { memcpy(d, s, n); d[n] = 0; }
    return d;
}

/* Diagnostics */
int lowmem_is_low(const void *p) { return (uintptr_t)p < LOW_END; }
