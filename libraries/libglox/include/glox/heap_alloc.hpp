#pragma once

#ifndef USE_MODULES
#include "alloc_types.hpp"
#include "math.hpp"
#include "mutex.hpp"
#include "utilalgs.hpp"
#include <atomic>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <new>
#endif

GLOX_EXPORT
namespace glox {

template <typename T>
concept page_allocator = requires(T& alloc, void* ptr, std::size_t n) {
    { alloc.alloc(n) } -> std::same_as<alloc_handle<void>>;
    { alloc.dealloc(ptr, n) } -> std::same_as<void>;
    { alloc.grow(ptr, n, n) } -> std::same_as<alloc_handle<void>>;
    { alloc.shrink(ptr, n, n) } -> std::same_as<alloc_handle<void>>;
};

struct free_node
{
    free_node* next;
};

namespace heap_detail {
    enum class arena_kind : std::uint32_t
    {
        regular,
        huge
    };
    enum class page_kind : std::uint32_t
    {
        small,
        medium,
        large
    };

    inline constexpr unsigned MEDIUM_SUB_BINS_BITS = 2; // 4 sub-bins per octave

    template <
        std::size_t MaxSmallSize,
        std::size_t Granularity,
        std::size_t MaxMediumSize
    >
    consteval std::size_t total_bucket_count() noexcept
    {
        const std::size_t small_buckets = MaxSmallSize / Granularity;
        const std::size_t med_octaves
            = std::countr_zero(MaxMediumSize) - std::countr_zero(MaxSmallSize);
        return small_buckets + (med_octaves << MEDIUM_SUB_BINS_BITS);
    }

    // First member of every arena; found by masking any interior pointer.
    struct arena_base
    {
        arena_kind kind { arena_kind::regular };
    };
} // namespace heap_detail

// Page metadata (mimalloc's mi_page_t). Lives in the arena's metadata area,
// never inside the page memory itself.
struct page_meta
{
    // Three free lists, as in mimalloc:
    //  free        - owner's allocation list (fast path pops from here)
    //  local_free  - frees performed by the owner
    //  xthread_free- frees performed by other threads/CPUs (atomic)
    free_node* free { nullptr };
    free_node* local_free { nullptr };
    std::atomic<free_node*> xthread_free { nullptr };

    // Owner token (address of the owning thread_cache). 0 = abandoned/none.
    std::atomic<std::uintptr_t> owner { 0 };

    page_meta* next { nullptr }; // cache queue / abandoned list / deferred list
    heap_detail::arena_base* arena { nullptr };
    char* start { nullptr };
    std::size_t block_size {
        0
    }; // small/medium: block size; large: payload bytes

    // Blocks handed out and not yet returned to a page list the owner can see.
    // Remote frees are counted only when the owner collects them.
    std::uint32_t used { 0 };
    std::uint32_t capacity { 0 }; // blocks that fit in the page
    std::uint32_t reserved { 0 }; // blocks carved into free lists so far
    std::uint32_t slice_idx { 0 };
    std::uint32_t slice_count { 1 };
    std::uint32_t bucket { 0 };
    heap_detail::page_kind kind { heap_detail::page_kind::small };
};

namespace heap_detail {

    // Arena: one aligned region carved into SliceSize slices. Metadata (slice
    // map + page_meta array) sits at the start of the arena.
    template <std::size_t SliceSize, std::size_t ArenaSize, typename Mutex>
    struct regular_arena : arena_base
    {
        static constexpr std::uint32_t NUM_SLICES
            = static_cast<std::uint32_t>(ArenaSize / SliceSize);

        Mutex lock;
        regular_arena* next_arena {
            nullptr
        }; // registry, written under heap lock
        void* raw { nullptr };
        std::size_t raw_size { 0 };
        page_meta* map[NUM_SLICES] { }; // slice -> page (nullptr = free)
        page_meta metas[NUM_SLICES]; // indexed by the page's first slice

        regular_arena() noexcept
        {
            kind = arena_kind::regular;
        }

        static constexpr std::uint32_t header_slices() noexcept
        {
            return static_cast<std::uint32_t>(
                (sizeof(regular_arena) + SliceSize - 1) / SliceSize
            );
        }

        page_meta* alloc_span(std::uint32_t k) noexcept
        {
            lock.lock();
            std::uint32_t i = header_slices();
            while (i + k <= NUM_SLICES) {
                std::uint32_t j = 0;
                while (j < k && map[i + j] == nullptr)
                    ++j;
                if (j == k) {
                    page_meta* m = new (&metas[i]) page_meta();
                    m->arena = this;
                    m->slice_idx = i;
                    m->slice_count = k;
                    m->start = reinterpret_cast<char*>(this) + i * SliceSize;
                    for (std::uint32_t t = 0; t < k; ++t)
                        map[i + t] = m;
                    lock.unlock();
                    return m;
                }
                i += j + 1;
            }
            lock.unlock();
            return nullptr;
        }

        void free_span(page_meta* m) noexcept
        {
            lock.lock();
            for (std::uint32_t t = 0; t < m->slice_count; ++t)
                map[m->slice_idx + t] = nullptr;
            lock.unlock();
        }

        // In-place grow/shrink of a multi-slice page. The page_meta never
        // moves.
        bool try_resize(page_meta* m, std::uint32_t new_k) noexcept
        {
            lock.lock();
            const std::uint32_t cur = m->slice_count;
            bool ok = true;
            if (new_k > cur) {
                if (m->slice_idx + new_k > NUM_SLICES) {
                    ok = false;
                } else {
                    for (std::uint32_t t = cur; t < new_k; ++t)
                        if (map[m->slice_idx + t] != nullptr) {
                            ok = false;
                            break;
                        }
                }
                if (ok)
                    for (std::uint32_t t = cur; t < new_k; ++t)
                        map[m->slice_idx + t] = m;
            } else {
                for (std::uint32_t t = new_k; t < cur; ++t)
                    map[m->slice_idx + t] = nullptr;
            }
            if (ok) {
                m->slice_count = new_k;
                m->block_size = static_cast<std::size_t>(new_k) * SliceSize;
            }
            lock.unlock();
            return ok;
        }
    };

    // Dedicated arena for one huge allocation (too big for a regular arena).
    // Aligned to ArenaSize so ptr_page() works the same way.
    struct huge_arena : arena_base
    {
        page_meta page;
        void* raw { nullptr };
        std::size_t raw_size { 0 };
        std::size_t total { 0 };

        huge_arena() noexcept
        {
            kind = arena_kind::huge;
        }
    };

} // namespace heap_detail

// Thread/CPU-local cache. Only the owner touches `pages`; `atomic_stash` is
// the only thing alloc_atomic()/dealloc_atomic() ever touch.
template <std::size_t NumBuckets>
struct thread_cache
{
    page_meta* pages[NumBuckets] { }; // per-bucket page queue, head = current
    std::atomic<free_node*> atomic_stash[NumBuckets] { };
};

template <typename T, std::size_t NumBuckets>
concept thread_routines = requires(T t) {
    { t.get_tcache() } -> std::same_as<thread_cache<NumBuckets>&>;
};

template <
    page_allocator PAllocator,
    typename TRoutines,
    std::size_t MaxSmallSize = 2048,
    std::size_t Granularity = 8,
    std::size_t PageSize = 65536, // slice / small-page size
    std::size_t ArenaSize = 32u * 1024 * 1024,
    glox::mutex_type Mutex = glox::spinlock,
    std::size_t MaxMediumSize = 512u * 1024
>
    requires thread_routines<
        TRoutines,
        heap_detail::
            total_bucket_count<MaxSmallSize, Granularity, MaxMediumSize>()
    >
class heap_allocator
{
public:
    static constexpr std::size_t num_buckets = heap_detail::
        total_bucket_count<MaxSmallSize, Granularity, MaxMediumSize>();

private:
    static constexpr std::size_t SMALL_BUCKETS = MaxSmallSize / Granularity;
    static constexpr std::size_t NUM_BUCKETS = num_buckets;
    static constexpr unsigned SUB_BINS_BITS = heap_detail::MEDIUM_SUB_BINS_BITS;
    static constexpr unsigned MIN_MED_LOG2 = std::countr_zero(MaxSmallSize);

    using arena_t = heap_detail::regular_arena<PageSize, ArenaSize, Mutex>;
    using huge_t = heap_detail::huge_arena;
    using tcache_t = thread_cache<NUM_BUCKETS>;

    static_assert(std::has_single_bit(PageSize));
    static_assert(std::has_single_bit(ArenaSize));
    static_assert(std::has_single_bit(Granularity));
    static_assert(std::has_single_bit(MaxSmallSize));
    static_assert(std::has_single_bit(MaxMediumSize));
    static_assert(Granularity >= sizeof(free_node));
    static_assert(MaxSmallSize >= 4 * Granularity);
    static_assert(MaxSmallSize <= PageSize / 2);
    static_assert(MaxMediumSize >= MaxSmallSize);
    static_assert(ArenaSize >= 8 * PageSize);
    static_assert(std::atomic<free_node*>::is_always_lock_free);
    static_assert(std::atomic<std::uintptr_t>::is_always_lock_free);

    static constexpr std::size_t MAX_REQUEST = ~std::size_t(0) >> 1;
    static constexpr std::uint32_t MAX_ARENA_SPAN
        = arena_t::NUM_SLICES / 4; // larger goes to a huge arena
    static constexpr std::uint32_t MAX_MEDIUM_SPAN = glox::min(
        MAX_ARENA_SPAN / 2,
        glox::max(
            MAX_ARENA_SPAN / 8,
            static_cast<std::uint32_t>(
                (MaxMediumSize * 2 + PageSize - 1) / PageSize
            )
        )
    );
    static constexpr std::uint32_t MEDIUM_TARGET_BLOCKS = 8;
    static constexpr std::uint32_t EXTEND_BATCH = 32;

    static_assert(MaxMediumSize <= std::size_t(MAX_MEDIUM_SPAN) * PageSize);

    [[no_unique_address]] PAllocator palloc;
    [[no_unique_address]] TRoutines troutines;

    // Shared state. `shared_lock` guards arena creation and abandoned lists.
    Mutex shared_lock;
    std::atomic<arena_t*> arenas { nullptr };
    page_meta* abandoned[NUM_BUCKETS] { };
    std::atomic<page_meta*> deferred { nullptr }; // large frees from atomic ctx

    // ---- helpers -----------------------------------------------------------

    // Small: linear 8B steps up to MaxSmallSize.
    // Medium: 4 sub-bins per octave (1.25x, 1.50x, 1.75x, 2.00x) up to
    // MaxMediumSize, bounding internal fragmentation to < 20%.
    static constexpr std::size_t get_bucket_index(std::size_t size) noexcept
    {
        if (size <= MaxSmallSize)
            return size == 0 ? 0 : (size - 1) / Granularity;
        const std::size_t x = size - 1;
        const unsigned e = static_cast<unsigned>(std::bit_width(x)) - 1;
        const std::size_t sub = (x >> (e - SUB_BINS_BITS))
                              & ((std::size_t(1) << SUB_BINS_BITS) - 1);
        return SMALL_BUCKETS
             + ((static_cast<std::size_t>(e - MIN_MED_LOG2) << SUB_BINS_BITS)
                 | sub);
    }

    static constexpr std::size_t get_block_size(std::size_t b) noexcept
    {
        if (b < SMALL_BUCKETS)
            return (b + 1) * Granularity;
        const std::size_t med_idx = b - SMALL_BUCKETS;
        const unsigned e
            = MIN_MED_LOG2 + static_cast<unsigned>(med_idx >> SUB_BINS_BITS);
        const std::size_t sub
            = med_idx & ((std::size_t(1) << SUB_BINS_BITS) - 1);
        return ((std::size_t(1) << SUB_BINS_BITS) + sub + 1)
            << (e - SUB_BINS_BITS);
    }

    static constexpr bool is_medium_bucket(std::size_t b) noexcept
    {
        return b >= SMALL_BUCKETS;
    }

    // Span enough slices so medium pages hold multiple blocks without tail
    // waste.
    static constexpr std::uint32_t medium_span_slices(
        std::size_t block_size
    ) noexcept
    {
        const std::size_t desired = block_size * MEDIUM_TARGET_BLOCKS;
        const std::size_t k = (desired + PageSize - 1) / PageSize;
        return static_cast<std::uint32_t>(glox::min(
            glox::max(k, std::size_t(1)), std::size_t(MAX_MEDIUM_SPAN)
        ));
    }

    static std::uintptr_t token(tcache_t& tc) noexcept
    {
        return reinterpret_cast<std::uintptr_t>(&tc);
    }

    static bool normalize(
        std::size_t n,
        std::size_t& alignment,
        std::size_t& alignedN
    ) noexcept
    {
        if (!std::has_single_bit(alignment) || alignment > PageSize
            || n > MAX_REQUEST)
            return false;
        alignment = glox::max(alignment, Granularity);
        alignedN = glox::align_up(glox::max(n, alignment), alignment);
        return true;
    }

    static constexpr bool is_cached(std::size_t alignedN) noexcept
    {
        return alignedN <= MaxMediumSize;
    }

    // mi_ptr_page: arena by masking, page by slice map. No in-page header.
    static page_meta* ptr_page(const void* p) noexcept
    {
        const auto addr = reinterpret_cast<std::uintptr_t>(p);
        const auto baseAddr = addr & ~(std::uintptr_t(ArenaSize) - 1);
        auto* base = reinterpret_cast<heap_detail::arena_base*>(baseAddr);
        if (base->kind == heap_detail::arena_kind::huge)
            return &static_cast<huge_t*>(base)->page;
        return static_cast<arena_t*>(base)->map[(addr - baseAddr) / PageSize];
    }

    static void push_chain(
        std::atomic<free_node*>& head,
        free_node* first,
        free_node* last
    ) noexcept
    {
        free_node* old = head.load(std::memory_order_relaxed);
        do {
            last->next = old;
        } while (!head.compare_exchange_weak(
            old, first, std::memory_order_release, std::memory_order_relaxed
        ));
    }

    // Free into a page we do not own (or from atomic context). Touches only the
    // page's atomic list; the block stays counted in `used` until the owner
    // collects it, so the page cannot be reclaimed underneath us.
    static void remote_free(page_meta* pg, void* ptr) noexcept
    {
        auto* node = static_cast<free_node*>(ptr);
        push_chain(pg->xthread_free, node, node);
    }

    // Owner only. Folds remote frees into local_free and fixes `used`.
    static void collect(page_meta* pg) noexcept
    {
        if (pg->xthread_free.load(std::memory_order_relaxed) != nullptr) {
            free_node* x
                = pg->xthread_free.exchange(nullptr, std::memory_order_acquire);
            if (x != nullptr) {
                std::uint32_t n = 1;
                free_node* tail = x;
                while (tail->next != nullptr) {
                    tail = tail->next;
                    ++n;
                }
                tail->next = pg->local_free;
                pg->local_free = x;
                pg->used -= n;
            }
        }
        if (pg->free == nullptr && pg->local_free != nullptr) {
            pg->free = pg->local_free;
            pg->local_free = nullptr;
        }
    }

    // Lazily carve more blocks so a fresh page does not touch all its memory.
    // For medium pages, cap carving to ~PageSize bytes per batch so large
    // multi-slice pages don't fault untouched slices prematurely.
    static void extend(page_meta* pg) noexcept
    {
        const std::uint32_t remaining = pg->capacity - pg->reserved;
        if (remaining == 0)
            return;
        const std::uint32_t batch
            = pg->kind == heap_detail::page_kind::medium
                ? glox::max(
                      std::uint32_t(1),
                      static_cast<std::uint32_t>(PageSize / pg->block_size)
                  )
                : EXTEND_BATCH;
        const std::uint32_t n = glox::min(batch, remaining);
        char* p = pg->start
                + static_cast<std::size_t>(pg->reserved) * pg->block_size;
        auto* head = reinterpret_cast<free_node*>(p);
        for (std::uint32_t i = 0; i + 1 < n; ++i) {
            auto* node = reinterpret_cast<free_node*>(p);
            p += pg->block_size;
            node->next = reinterpret_cast<free_node*>(p);
        }
        reinterpret_cast<free_node*>(p)->next = nullptr;
        pg->free = head;
        pg->reserved += n;
    }

    static bool has_room(const page_meta* pg) noexcept
    {
        return pg->free != nullptr || pg->reserved < pg->capacity;
    }

    static void* pop(page_meta* pg) noexcept
    {
        if (pg->free == nullptr)
            extend(pg);
        free_node* b = pg->free;
        pg->free = b->next;
        pg->used++;
        return b;
    }

    // ---- arena / region management ----------------------------------------

    void* reserve_region(std::size_t size, void*& raw, std::size_t& raw_size)
    {
        auto h = palloc.alloc(size);
        if (!h.ptr)
            return nullptr;
        const auto mask = std::uintptr_t(ArenaSize) - 1;
        if ((reinterpret_cast<std::uintptr_t>(h.ptr) & mask) == 0) {
            raw = h.ptr;
            raw_size = size;
            return h.ptr;
        }
        palloc.dealloc(h.ptr, size);
        h = palloc.alloc(size + ArenaSize);
        if (!h.ptr)
            return nullptr;
        raw = h.ptr;
        raw_size = size + ArenaSize;
        auto aligned = (reinterpret_cast<std::uintptr_t>(h.ptr) + mask) & ~mask;
        return reinterpret_cast<void*>(aligned);
    }

    arena_t* create_arena()
    {
        void* raw = nullptr;
        std::size_t rawSize = 0;
        void* base = reserve_region(ArenaSize, raw, rawSize);
        if (!base)
            return nullptr;
        auto* a = new (base) arena_t();
        a->raw = raw;
        a->raw_size = rawSize;
        return a;
    }

    page_meta* acquire_span(std::uint32_t k)
    {
        for (auto* a = arenas.load(std::memory_order_acquire); a;
            a = a->next_arena)
            if (page_meta* p = a->alloc_span(k))
                return p;

        shared_lock.lock();
        // Another CPU may have added an arena while we waited.
        for (auto* a = arenas.load(std::memory_order_acquire); a;
            a = a->next_arena)
            if (page_meta* p = a->alloc_span(k)) {
                shared_lock.unlock();
                return p;
            }
        arena_t* a = create_arena();
        if (a) {
            a->next_arena = arenas.load(std::memory_order_relaxed);
            arenas.store(a, std::memory_order_release);
        }
        shared_lock.unlock();
        return a ? a->alloc_span(k) : nullptr;
    }

    void release_page(page_meta* pg) noexcept
    {
        static_cast<arena_t*>(pg->arena)->free_span(pg);
    }

    void release_large(page_meta* pg)
    {
        if (pg->arena->kind == heap_detail::arena_kind::huge) {
            auto* h = static_cast<huge_t*>(pg->arena);
            palloc.dealloc(h->raw, h->raw_size);
        } else {
            static_cast<arena_t*>(pg->arena)->free_span(pg);
        }
    }

    void defer_large(page_meta* pg) noexcept
    {
        page_meta* old = deferred.load(std::memory_order_relaxed);
        do {
            pg->next = old;
        } while (!deferred.compare_exchange_weak(
            old, pg, std::memory_order_release, std::memory_order_relaxed
        ));
    }

    void drain_deferred()
    {
        if (deferred.load(std::memory_order_relaxed) == nullptr)
            return;
        page_meta* pg = deferred.exchange(nullptr, std::memory_order_acquire);
        while (pg) {
            page_meta* nx = pg->next; // read first: huge pages vanish below
            release_large(pg);
            pg = nx;
        }
    }

    page_meta* take_abandoned(std::size_t b) noexcept
    {
        shared_lock.lock();
        page_meta* pg = abandoned[b];
        if (pg)
            abandoned[b] = pg->next;
        shared_lock.unlock();
        return pg;
    }

    void abandon(page_meta* pg, std::size_t b) noexcept
    {
        pg->owner.store(0, std::memory_order_release);
        shared_lock.lock();
        pg->next = abandoned[b];
        abandoned[b] = pg;
        shared_lock.unlock();
    }

    // ---- allocation slow paths --------------------------------------------

    void* alloc_cached_slow(tcache_t& tc, std::size_t b)
    {
        drain_deferred();

        // 1. Existing pages: collect remote frees, pick a page with room,
        //    release surplus empty pages.
        page_meta* cand = nullptr;
        page_meta** candLink = nullptr;
        page_meta** link = &tc.pages[b];
        while (page_meta* pg = *link) {
            collect(pg);
            if (pg->used == 0 && cand != nullptr) {
                *link = pg->next;
                release_page(pg);
                continue;
            }
            if (cand == nullptr && has_room(pg)) {
                cand = pg;
                candLink = link;
            }
            link = &pg->next;
        }
        if (cand) {
            if (candLink != &tc.pages[b]) {
                *candLink = cand->next;
                cand->next = tc.pages[b];
                tc.pages[b] = cand;
            }
            return pop(cand);
        }

        // 2. Adopt pages abandoned by other caches.
        const std::uintptr_t tok = token(tc);
        while (page_meta* pg = take_abandoned(b)) {
            pg->owner.store(tok, std::memory_order_release);
            collect(pg);
            pg->next = tc.pages[b];
            tc.pages[b] = pg;
            if (has_room(pg))
                return pop(pg);
        }

        // 3. Fresh page (1 slice for small, multi-slice span for medium).
        const bool medium = is_medium_bucket(b);
        const std::size_t blk_size = get_block_size(b);
        const std::uint32_t span = medium ? medium_span_slices(blk_size) : 1;
        page_meta* pg = acquire_span(span);
        if (!pg)
            return nullptr;
        pg->kind = medium ? heap_detail::page_kind::medium
                          : heap_detail::page_kind::small;
        pg->bucket = static_cast<std::uint32_t>(b);
        pg->block_size = blk_size;
        pg->capacity = static_cast<std::uint32_t>(
            (static_cast<std::size_t>(span) * PageSize) / blk_size
        );
        pg->owner.store(tok, std::memory_order_release);
        pg->next = tc.pages[b];
        tc.pages[b] = pg;
        return pop(pg);
    }

    alloc_handle<void> alloc_huge(std::size_t n, std::size_t alignment)
    {
        const std::size_t offset
            = align_up(sizeof(huge_t), glox::max(alignment, std::size_t(64)));
        const std::size_t total = align_up(n + offset, PageSize);
        void* raw = nullptr;
        std::size_t rawSize = 0;
        void* base = reserve_region(total, raw, rawSize);
        if (!base)
            return { nullptr, 0 };

        auto* h = new (base) huge_t();
        h->raw = raw;
        h->raw_size = rawSize;
        h->total = total;
        page_meta& pg = h->page;
        pg.arena = h;
        pg.kind = heap_detail::page_kind::large;
        pg.start = static_cast<char*>(base) + offset;
        pg.block_size = total - offset;
        pg.used = pg.capacity = pg.reserved = 1;
        return { pg.start, pg.block_size };
    }

    alloc_handle<void> alloc_large(std::size_t n, std::size_t alignment)
    {
        drain_deferred();
        const std::size_t k = n == 0 ? 1 : (n + PageSize - 1) / PageSize;
        if (k > MAX_ARENA_SPAN)
            return alloc_huge(n, alignment);

        page_meta* pg = acquire_span(static_cast<std::uint32_t>(k));
        if (!pg)
            return { nullptr, 0 };
        pg->kind = heap_detail::page_kind::large;
        pg->block_size = k * PageSize;
        pg->used = pg->capacity = pg->reserved = 1;
        return { pg->start, pg->block_size };
    }

public:
    heap_allocator() = default;

    alloc_handle<void> alloc(std::size_t n, std::size_t alignment = Granularity)
    {
        std::size_t alignedN = 0;
        if (!normalize(n, alignment, alignedN))
            return { nullptr, 0 };
        if (!is_cached(alignedN))
            return alloc_large(n, alignment);

        const std::size_t b = get_bucket_index(alignedN);
        auto& tc = troutines.get_tcache();

        // Fast path: entirely page-local, no atomics, no locks.
        if (page_meta* pg = tc.pages[b]) {
            if (free_node* blk = pg->free) {
                pg->free = blk->next;
                pg->used++;
                return { blk, get_block_size(b) };
            }
        }
        void* p = alloc_cached_slow(tc, b);
        if (!p)
            return { nullptr, 0 };
        return { p, get_block_size(b) };
    }

    std::size_t usable_size(void* ptr) noexcept
    {
        if (!ptr)
            return 0;
        page_meta* pg = ptr_page(ptr);
        if (pg->kind == heap_detail::page_kind::large)
            return static_cast<std::size_t>(
                pg->start + pg->block_size - static_cast<char*>(ptr)
            );
        return pg->block_size;
    }

    void dealloc(void* ptr)
    {
        if (!ptr)
            return;
        page_meta* pg = ptr_page(ptr);
        if (pg->kind == heap_detail::page_kind::large) {
            drain_deferred();
            release_large(pg);
            return;
        }
        auto& tc = troutines.get_tcache();
        if (pg->owner.load(std::memory_order_relaxed) == token(tc)) {
            auto* node = static_cast<free_node*>(ptr);
            node->next = pg->local_free;
            pg->local_free = node;
            pg->used--;

            // Keep the active head medium page cached for reuse hysteresis,
            // but return completely empty non-head medium pages promptly.
            if (pg->kind == heap_detail::page_kind::medium && pg->used == 0
                && tc.pages[pg->bucket] != pg) {
                page_meta** link = &tc.pages[pg->bucket];
                while (*link && *link != pg)
                    link = &(*link)->next;
                if (*link == pg) {
                    *link = pg->next;
                    release_page(pg);
                }
            }
        } else {
            remote_free(pg, ptr);
        }
    }

    void dealloc(
        void* ptr,
        [[maybe_unused]] std::size_t n,
        [[maybe_unused]] std::size_t alignment = Granularity
    )
    {
        dealloc(ptr);
    }

    // ---- interrupt-safe path ------------------------------------------------
    // Lock-free and never touches non-atomic cache state. alloc_atomic() only
    // serves blocks pre-staged with reserve_atomic(); if none are available it
    // returns {nullptr, 0} (no fallback to alloc()).

    // Normal context: stage up to `count` blocks of size `n`. Returns how many.
    std::size_t reserve_atomic(
        std::size_t n,
        std::size_t count,
        std::size_t alignment = Granularity
    )
    {
        std::size_t alignedN = 0;
        if (!normalize(n, alignment, alignedN) || !is_cached(alignedN))
            return 0;
        const std::size_t b = get_bucket_index(alignedN);
        auto& stash = troutines.get_tcache().atomic_stash[b];
        std::size_t got = 0;
        while (got < count) {
            auto h = alloc(n, alignment); // counted as in use: pins its page
            if (!h.ptr)
                break;
            auto* node = static_cast<free_node*>(h.ptr);
            push_chain(stash, node, node);
            ++got;
        }
        return got;
    }

    alloc_handle<void> alloc_atomic(
        std::size_t n,
        std::size_t alignment = Granularity
    )
    {
        std::size_t alignedN = 0;
        if (!normalize(n, alignment, alignedN) || !is_cached(alignedN))
            return { nullptr, 0 };
        const std::size_t b = get_bucket_index(alignedN);
        auto& stash = troutines.get_tcache().atomic_stash[b];

        // Take the whole stack (ABA-free), keep one, push the rest back.
        free_node* all = stash.exchange(nullptr, std::memory_order_acquire);
        if (!all)
            return { nullptr, 0 };
        if (free_node* rest = all->next) {
            free_node* last = rest;
            while (last->next)
                last = last->next;
            push_chain(stash, rest, last);
        }
        return { all, get_block_size(b) };
    }

    void dealloc_atomic(void* ptr) noexcept
    {
        if (!ptr)
            return;
        page_meta* pg = ptr_page(ptr);
        if (pg->kind == heap_detail::page_kind::large)
            defer_large(pg); // released on the next normal-context alloc/free
        else
            remote_free(pg, ptr); // owner folds it in during collect()
    }

    // Call when a thread/CPU cache goes away. Empty pages return to the arena;
    // the rest are abandoned so another cache can adopt them.
    void release_cache(tcache_t& tc)
    {
        for (std::size_t b = 0; b < NUM_BUCKETS; ++b) {
            free_node* s = tc.atomic_stash[b].exchange(
                nullptr, std::memory_order_acq_rel
            );
            while (s) {
                free_node* nx = s->next;
                remote_free(ptr_page(s), s);
                s = nx;
            }
        }
        for (std::size_t b = 0; b < NUM_BUCKETS; ++b) {
            page_meta* pg = tc.pages[b];
            tc.pages[b] = nullptr;
            while (pg) {
                page_meta* nx = pg->next;
                collect(pg);
                if (pg->used == 0)
                    release_page(pg);
                else
                    abandon(pg, b);
                pg = nx;
            }
        }
    }

    // ---- resizing -----------------------------------------------------------

    void* grow_inplace(void* ptr, std::size_t new_size)
    {
        return (ptr && new_size <= usable_size(ptr)) ? ptr : nullptr;
    }

    void* shrink_inplace(void* ptr, std::size_t new_size)
    {
        return (ptr && new_size <= usable_size(ptr)) ? ptr : nullptr;
    }

    alloc_handle<void> grow(
        void* ptr,
        std::size_t new_size,
        std::size_t alignment = Granularity
    )
    {
        if (!ptr)
            return alloc(new_size, alignment);
        if (new_size == 0) {
            dealloc(ptr);
            return { nullptr, 0 };
        }
        const std::size_t cur = usable_size(ptr);
        if (new_size <= cur)
            return { ptr, cur };

        // Arena-backed large pages grow in place: the page_meta never moves.
        page_meta* pg = ptr_page(ptr);
        if (pg->kind == heap_detail::page_kind::large
            && pg->arena->kind == heap_detail::arena_kind::regular
            && static_cast<char*>(ptr) == pg->start
            && new_size <= MAX_REQUEST) {
            const std::size_t k = (new_size + PageSize - 1) / PageSize;
            if (k <= MAX_ARENA_SPAN
                && static_cast<arena_t*>(pg->arena)->try_resize(
                    pg, static_cast<std::uint32_t>(k)
                ))
                return { ptr, pg->block_size };
        }

        // Otherwise move. Huge arenas are never resized through palloc: it may
        // return a base that is not ArenaSize-aligned, which would break
        // ptr_page(). A new block gets fresh, correct metadata.
        auto nh = alloc(new_size, alignment);
        if (nh.ptr) {
            glox::memcpy(nh.ptr, ptr, cur);
            dealloc(ptr);
        }
        return nh;
    }

    alloc_handle<void> shrink(
        void* ptr,
        std::size_t new_size,
        std::size_t alignment = Granularity
    )
    {
        if (!ptr)
            return { nullptr, 0 };
        if (new_size == 0) {
            dealloc(ptr);
            return { nullptr, 0 };
        }
        const std::size_t cur = usable_size(ptr);
        if (new_size >= cur)
            return { ptr, cur };
        page_meta* pg = ptr_page(ptr);

        if (pg->kind == heap_detail::page_kind::large
            && static_cast<char*>(ptr) == pg->start) {
            if (pg->arena->kind == heap_detail::arena_kind::regular) {
                const std::size_t k = (new_size + PageSize - 1) / PageSize;
                if (k < pg->slice_count) {
                    static_cast<arena_t*>(pg->arena)->try_resize(
                        pg, static_cast<std::uint32_t>(k)
                    );
                    return { ptr, pg->block_size };
                }
            } else if (cur - new_size >= PageSize) {
                // Huge: only worth moving if we give back real memory.
                auto nh = alloc(new_size, alignment);
                if (nh.ptr) {
                    glox::memcpy(nh.ptr, ptr, new_size);
                    dealloc(ptr);
                    return nh;
                }
            }
        } else if (
            pg->kind == heap_detail::page_kind::medium
            && cur - new_size >= PageSize
        ) {
            // Medium: move to a smaller bucket when reclaiming >= 1 slice.
            auto nh = alloc(new_size, alignment);
            if (nh.ptr) {
                glox::memcpy(nh.ptr, ptr, new_size);
                dealloc(ptr);
                return nh;
            }
        }
        // Small blocks: not worth moving to a smaller bucket.
        return { ptr, cur };
    }
};

} // namespace glox
