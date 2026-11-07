// GPU buffer memory kept apart from the malloc heap.
//
// libdrm_nouveau backs every buffer object with memalign(0x1000, size) from the process heap and
// returns it with free() once the GPU is done. Emulators (Ryujinx) may write render-target data
// back into a buffer's guest memory after it was unmapped; with the memory already recycled by
// malloc, that overwrote heap chunk headers (0xFF000000 pixels in the free lists). Page-aligned,
// page-sized allocations are served from a separate pool whose bookkeeping lives outside the
// pool, and freed blocks wait in a quarantine before reuse. Everything else goes to newlib.
// Atmosphere means real hardware, which has no such writes: the pool stays off there, and costs
// neither its reservation nor the fill and check on every free.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <switch.h>

void* __real_memalign(size_t align, size_t size);
void __real_free(void* ptr);

#define POOL_PAGE 0x1000u
#define POOL_MAX_PAGES (384u * 1024u * 1024u / POOL_PAGE)
#define QUARANTINE_SLOTS 1024u
#define QUARANTINE_PAGES (48u * 1024u * 1024u / POOL_PAGE)
// Freed blocks are filled with this byte and checked when they leave the quarantine, which
// reports whether something still wrote to a buffer after the driver released it.
#define POOL_POISON 0xA5u

static Mutex s_lock;
static uint8_t* s_base;
static uint32_t s_pages;
static uint32_t s_cursor;
static int s_state; // 0 untried, 1 active, -1 unavailable
static uint64_t s_used[POOL_MAX_PAGES / 64u];
static uint32_t s_len[POOL_MAX_PAGES]; // block length in pages, at its first page

static struct
{
    uint32_t page;
    uint32_t count;
} s_quar[QUARANTINE_SLOTS];
static uint32_t s_quarHead, s_quarCount, s_quarPages;
static unsigned s_dirtyReports;

static int page_used(uint32_t p)
{
    return (int)((s_used[p >> 6] >> (p & 63u)) & 1u);
}

static void mark(uint32_t p, uint32_t n, int used)
{
    for (uint32_t i = p; i < p + n; ++i)
    {
        if (used)
            s_used[i >> 6] |= 1ull << (i & 63u);
        else
            s_used[i >> 6] &= ~(1ull << (i & 63u));
    }
}

static void pool_init(void)
{
    static const uint32_t sizesMiB[] = {384u, 256u, 128u};
    if (hosversionIsAtmosphere())
    {
        s_state = -1;
        fprintf(stderr, "[nx] GPU buffer pool off (Atmosphere)\n");
        return;
    }
    for (unsigned i = 0; i < sizeof sizesMiB / sizeof sizesMiB[0]; ++i)
    {
        const size_t bytes = (size_t)sizesMiB[i] * 1024u * 1024u;
        s_base = __real_memalign(POOL_PAGE, bytes);
        if (s_base != NULL)
        {
            s_pages = (uint32_t)(bytes / POOL_PAGE);
            s_state = 1;
            fprintf(stderr, "[nx] GPU buffer pool: %u MiB at %p\n", sizesMiB[i], (void*)s_base);
            return;
        }
    }
    s_state = -1;
    fprintf(stderr, "[nx] GPU buffer pool unavailable; buffers use the heap\n");
}

static int find_run(uint32_t n, uint32_t from, uint32_t to, uint32_t* out)
{
    uint32_t run = 0;
    for (uint32_t p = from; p < to; ++p)
    {
        // Skip whole free or used words quickly.
        if ((p & 63u) == 0 && p + 64u <= to)
        {
            const uint64_t w = s_used[p >> 6];
            if (w == 0)
            {
                run += 64u;
                if (run >= n)
                {
                    *out = p + 64u - run;
                    return 1;
                }
                p += 63u;
                continue;
            }
            if (w == ~0ull)
            {
                run = 0;
                p += 63u;
                continue;
            }
        }
        if (page_used(p))
            run = 0;
        else if (++run == n)
        {
            *out = p + 1u - n;
            return 1;
        }
    }
    return 0;
}

static void check_poison(uint32_t page, uint32_t count)
{
    const uint64_t* q = (const uint64_t*)(s_base + (size_t)page * POOL_PAGE);
    const size_t words = (size_t)count * POOL_PAGE / sizeof(uint64_t);
    const uint64_t poison = 0x0101010101010101ull * POOL_POISON;
    for (size_t i = 0; i < words; ++i)
    {
        if (q[i] != poison)
        {
            if (s_dirtyReports < 32u)
            {
                ++s_dirtyReports;
                fprintf(stderr,
                        "[nx] GPU buffer %p (%u pages) written after free: +0x%zx = 0x%016llx\n",
                        (void*)(s_base + (size_t)page * POOL_PAGE), count, i * sizeof(uint64_t),
                        (unsigned long long)q[i]);
            }
            return;
        }
    }
}

static void release_oldest(void)
{
    const uint32_t slot = (s_quarHead + QUARANTINE_SLOTS - s_quarCount) % QUARANTINE_SLOTS;
    check_poison(s_quar[slot].page, s_quar[slot].count);
    mark(s_quar[slot].page, s_quar[slot].count, 0);
    s_quarPages -= s_quar[slot].count;
    --s_quarCount;
}

static void* pool_alloc(size_t size)
{
    const uint32_t n = (uint32_t)((size + POOL_PAGE - 1u) / POOL_PAGE);
    void* result = NULL;
    mutexLock(&s_lock);
    if (s_state == 0)
        pool_init();
    if (s_state == 1 && n > 0 && n <= s_pages)
    {
        uint32_t p;
        for (;;)
        {
            if (find_run(n, s_cursor, s_pages, &p) || find_run(n, 0, s_pages, &p))
                break;
            if (s_quarCount == 0)
            {
                p = UINT32_MAX;
                break;
            }
            release_oldest();
        }
        if (p != UINT32_MAX)
        {
            mark(p, n, 1);
            s_len[p] = n;
            s_cursor = p + n < s_pages ? p + n : 0;
            result = s_base + (size_t)p * POOL_PAGE;
        }
    }
    mutexUnlock(&s_lock);
    return result;
}

static int pool_owns(const void* ptr)
{
    return s_state == 1 && (const uint8_t*)ptr >= s_base &&
           (const uint8_t*)ptr < s_base + (size_t)s_pages * POOL_PAGE;
}

static void pool_free(void* ptr)
{
    mutexLock(&s_lock);
    const uint32_t p = (uint32_t)(((uint8_t*)ptr - s_base) / POOL_PAGE);
    const uint32_t n = s_len[p];
    if (n == 0 || (uint8_t*)ptr != s_base + (size_t)p * POOL_PAGE)
    {
        fprintf(stderr, "[nx] GPU buffer pool: bad free %p\n", ptr);
        mutexUnlock(&s_lock);
        return;
    }
    s_len[p] = 0;
    memset(ptr, POOL_POISON, (size_t)n * POOL_PAGE);
    if (s_quarCount == QUARANTINE_SLOTS)
        release_oldest();
    s_quar[s_quarHead].page = p;
    s_quar[s_quarHead].count = n;
    s_quarHead = (s_quarHead + 1u) % QUARANTINE_SLOTS;
    ++s_quarCount;
    s_quarPages += n;
    while (s_quarPages > QUARANTINE_PAGES && s_quarCount > 1)
        release_oldest();
    mutexUnlock(&s_lock);
}

void* __wrap_memalign(size_t align, size_t size)
{
    if (align == POOL_PAGE && size >= POOL_PAGE && (size & (POOL_PAGE - 1u)) == 0)
    {
        void* p = pool_alloc(size);
        if (p != NULL)
            return p;
    }
    return __real_memalign(align, size);
}

void __wrap_free(void* ptr)
{
    if (ptr != NULL && pool_owns(ptr))
        pool_free(ptr);
    else
        __real_free(ptr);
}
