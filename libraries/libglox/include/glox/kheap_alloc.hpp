#pragma once

#ifndef USE_MODULES
#include "alloc_types.hpp"
#include "buddy_alloc.hpp" // pfndb_accessor, pfn_descriptor
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

namespace kheap_detail {
    template <typename T>
    concept page_allocator = requires(T& alloc, void* ptr, std::size_t n) {
        { alloc.alloc(n) } -> std::same_as<alloc_handle<void>>;
        { alloc.dealloc(ptr, n) } -> std::same_as<void>;
        { alloc.grow(ptr, n, n) } -> std::same_as<alloc_handle<void>>;
        { alloc.shrink(ptr, n, n) } -> std::same_as<alloc_handle<void>>;
    };

    // Only used to thread blocks of the interrupt-safe stash together. Slab
    // free lists use 16-bit block indices instead (see page_meta).
    struct free_node
    {
        free_node* next;
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
        const std::size_t smallBuckets = MaxSmallSize / Granularity;
        const std::size_t medOctaves
            = std::countr_zero(MaxMediumSize) - std::countr_zero(MaxSmallSize);
        return smallBuckets + (medOctaves << MEDIUM_SUB_BINS_BITS);
    }

    // ---- debug policies -----------------------------------------------------
    // `enabled` : report misuse (double free, interior pointer, foreign
    //             pointer) through fail(). Misuse is *always* ignored (the
    //             free is dropped); the policy only decides whether to trap.
    // `deep`    : additionally walk the owner's free lists on local frees to
    //             catch double frees of slab blocks (O(capacity)).
    struct no_checks
    {
        static constexpr bool enabled = false;
        static constexpr bool deep = false;
        static void fail() noexcept { }
    };

    struct trap_checks
    {
        static constexpr bool enabled = true;
        static constexpr bool deep = false;
        [[noreturn]] static void fail() noexcept
        {
            __builtin_trap();
        }
    };

    struct deep_trap_checks : trap_checks
    {
        static constexpr bool deep = true;
    };

    // Unevaluated-only helper (declared, never defined).
    template <typename T>
    T& declref() noexcept;

    // The heap stores 64-bit tagged words in the pfndb. The accessor's
    // descriptor type must expose them losslessly, and the accessor must map
    // a virtual address to its pfn (pfn_to_virt alone is not enough).
    //
    // Encoding contract with the page allocator (see TAG_BUDDY below): raw
    // value 0 must be a state the buddy treats as "not free", and every raw
    // value whose tag != 0 must report is_buddy_free() == false.
    template <typename A>
    concept memdesc_accessor = requires(
        A& a,
        const A& ca,
        const void* p,
        std::uintptr_t pfn,
        std::uint64_t raw
    ) {
        { ca.virt_to_pfn(p) } -> std::convertible_to<std::uintptr_t>;
        { ca[pfn].raw() } -> std::convertible_to<std::uint64_t>;
        a[pfn] = std::remove_cvref_t<decltype(a[pfn])>::from_raw(raw);
    };

    // Size-class math. Single source of truth for the class and the
    // reciprocal table (class bodies cannot feed static constexpr members).
    template <std::size_t MaxSmallSize, std::size_t Granularity>
    struct size_classes
    {
        static constexpr std::size_t SMALL_BUCKETS = MaxSmallSize / Granularity;
        static constexpr unsigned SUB_BINS_BITS = MEDIUM_SUB_BINS_BITS;
        static constexpr unsigned MIN_MED_LOG2 = std::countr_zero(MaxSmallSize);

        // Small: linear 8B steps up to MaxSmallSize.
        // Medium: 4 sub-bins per octave (1.25x, 1.50x, 1.75x, 2.00x) up to
        // MaxMediumSize, bounding internal fragmentation to < 20%.
        static constexpr std::size_t bucket_index(std::size_t size) noexcept
        {
            if (size <= MaxSmallSize)
                return size == 0 ? 0 : (size - 1) / Granularity;
            const std::size_t x = size - 1;
            const unsigned e = static_cast<unsigned>(std::bit_width(x)) - 1;
            const std::size_t sub = (x >> (e - SUB_BINS_BITS))
                                  & ((std::size_t(1) << SUB_BINS_BITS) - 1);
            return SMALL_BUCKETS
                 + ((static_cast<std::size_t>(e - MIN_MED_LOG2)
                        << SUB_BINS_BITS)
                     | sub);
        }

        static constexpr std::size_t block_size(std::size_t b) noexcept
        {
            if (b < SMALL_BUCKETS)
                return (b + 1) * Granularity;
            const std::size_t medIdx = b - SMALL_BUCKETS;
            const unsigned e
                = MIN_MED_LOG2 + static_cast<unsigned>(medIdx >> SUB_BINS_BITS);
            const std::size_t sub
                = medIdx & ((std::size_t(1) << SUB_BINS_BITS) - 1);
            return ((std::size_t(1) << SUB_BINS_BITS) + sub + 1)
                << (e - SUB_BINS_BITS);
        }
    };

    // idx = (off * recip_mult) >> recip_shift == off / block_size for every
    // off < 2^OffsetBits. With s = OffsetBits + ceil(log2 d) and
    // m = ceil(2^s / d), the rounding error (m*d - 2^s) is < d <= 2^ceil(log2
    // d) so n * error < 2^s for all n < 2^OffsetBits, which makes the floor
    // exact.
    struct bucket_spec
    {
        std::uint32_t block_size;
        std::uint32_t recip_mult;
        std::uint8_t recip_shift;
    };

    template <std::size_t N>
    struct bucket_table
    {
        bucket_spec spec[N];
    };

    template <typename Classes, std::size_t NumBuckets, unsigned OffsetBits>
    consteval bucket_table<NumBuckets> make_bucket_table() noexcept
    {
        static_assert(OffsetBits <= 30, "reciprocal must fit 32 bits");
        bucket_table<NumBuckets> t { };
        for (std::size_t b = 0; b < NumBuckets; ++b) {
            const std::uint64_t d = Classes::block_size(b);
            const unsigned lg = static_cast<unsigned>(std::bit_width(d - 1));
            const unsigned shift = OffsetBits + lg;
            const std::uint64_t m = ((std::uint64_t(1) << shift) + d - 1) / d;
            t.spec[b] = bucket_spec { static_cast<std::uint32_t>(d),
                static_cast<std::uint32_t>(m),
                static_cast<std::uint8_t>(shift) };
        }
        return t;
    }
} // namespace kheap_detail

// Slab metadata. Lives in TAG_META pages carved into
// 48-byte slots, never inside the data pages. Blocks are addressed by 16-bit
// index (block = start + idx * block_size); the link of a free block is
// stored in its first two bytes.
struct alignas(8) kpage_meta
{
    static constexpr std::uint16_t EMPTY = 0xFFFF;

    kpage_meta* next { nullptr }; // cache queue / abandoned / meta pool
    char* start { nullptr }; // slab base virtual address
    std::uintptr_t owner { 0 }; // token(tc) of owner (0 = abandoned)
    std::uint32_t block_size { 0 }; // allocated block size in bytes
    std::uint16_t free_head { EMPTY }; // owner fast-path pop head
    std::uint16_t local_free_head { EMPTY }; // owner fast-path push head
    std::atomic<std::uint16_t> xthread_head { EMPTY }; // remote push head
    std::uint16_t used { 0 }; // live allocated blocks
    std::uint16_t capacity { 0 }; // max blocks in slab
    std::uint16_t reserved { 0 }; // blocks lazily carved so far
    std::uint16_t page_count { 1 }; // span length in pages
    std::uint16_t bucket { 0 }; // size-class index
    kheap_detail::page_kind kind { kheap_detail::page_kind::small };
};

static_assert(sizeof(kpage_meta) == 48);
static_assert(alignof(kpage_meta) == 8);
static_assert(std::atomic<std::uint16_t>::is_always_lock_free);

// Thread/CPU-local cache. Only the owner touches `pages` and `meta_free_list`;
// `atomic_stash` is the only thing alloc_atomic()/dealloc_atomic() ever touch.
template <std::size_t NumBuckets>
struct kthread_cache
{
    kpage_meta* pages[NumBuckets] { }; // per-bucket slab queue, head = current
    std::atomic<kheap_detail::free_node*> atomic_stash[NumBuckets] { };
    kpage_meta* meta_free_list { nullptr }; // owner-private page_meta slots
};

template <typename T, std::size_t NumBuckets>
concept kthread_routines = requires(T t) {
    { t.get_tcache() } -> std::same_as<kthread_cache<NumBuckets>&>;
};

// Kernel heap on top of a page allocator (buddy) and the pfndb.
//
// Nothing is stored in data pages and no page_meta is ever found by address
// arithmetic on user memory: ptr -> pfn -> pfndb word -> {slab | large | ...}.
//
// pfndb word (per PFN), tag in [4:0]:
//   TAG_BUDDY      0x00  page owned by palloc (free, or allocated but
//   unstamped)
//
//   TAG_META       0x01  PageSize page carved into page_meta slots
//
//   TAG_SLAB       0x02  [PageShift-1:5] slot index, [63:PageShift] meta page
//                        base (every PFN of a multi-page slab: identical word)
//   TAG_LARGE      0x03  head frame, [63:5] page_count
//   TAG_LARGE_TAIL 0x04  frame i >= 1 of a large span, [63:5] = i
// Metadata pointers keep bits [63:PageShift] verbatim, so LA57 (57-bit)
// canonical addresses are preserved.
//
// Contract on PAllocator::grow/shrink: they resize in place and return the
// *same* pointer on success; anything else (including {nullptr, 0}) means
// "unchanged, could not resize".
template <
    kheap_detail::page_allocator PAllocator,
    pfndb_accessor Accessor,
    typename TRoutines,
    glox::mutex_type Mutex = glox::spinlock,
    std::size_t PageShift = 12,
    std::size_t MaxSmallSize = 2048,
    std::size_t Granularity = 8,
    std::size_t MaxMediumSize = 65536,
    typename DebugChecks = kheap_detail::no_checks
>
    requires kthread_routines<
        TRoutines,
        kheap_detail::
            total_bucket_count<MaxSmallSize, Granularity, MaxMediumSize>()
    >
class kheap_allocator
{
public:
    static constexpr std::size_t PageSize = std::size_t(1) << PageShift;
    static constexpr std::size_t num_buckets = kheap_detail::
        total_bucket_count<MaxSmallSize, Granularity, MaxMediumSize>();

    static constexpr std::uint64_t TAG_BUDDY = Accessor::TAG_BUDDY;
    static constexpr std::uint64_t TAG_META = Accessor::TAG_META;
    static constexpr std::uint64_t TAG_SLAB = Accessor::TAG_SLAB;
    static constexpr std::uint64_t TAG_LARGE = Accessor::TAG_LARGE;
    static constexpr std::uint64_t TAG_LARGE_TAIL = Accessor::TAG_LARGE_TAIL;

private:
    static_assert(PageShift >= 12, "Page size must be at least 4 KiB");
    static_assert(
        kheap_detail::memdesc_accessor<Accessor>,
        "Accessor's descriptor needs from_raw()/raw() and the accessor needs "
        "virt_to_pfn()"
    );

    using desc_t = std::remove_cvref_t<
        decltype(kheap_detail::declref<Accessor>()[std::uintptr_t(0)])
    >;
    using tcache_t = kthread_cache<num_buckets>;
    using sizes_t = kheap_detail::size_classes<MaxSmallSize, Granularity>;

    static constexpr std::size_t SMALL_BUCKETS = sizes_t::SMALL_BUCKETS;
    static constexpr std::size_t NUM_BUCKETS = num_buckets;

    static_assert(std::has_single_bit(Granularity));
    static_assert(std::has_single_bit(MaxSmallSize));
    static_assert(std::has_single_bit(MaxMediumSize));
    static_assert(Granularity >= sizeof(kheap_detail::free_node));
    static_assert(Granularity >= sizeof(std::uint16_t));
    static_assert(MaxSmallSize >= 4 * Granularity);
    static_assert(MaxSmallSize <= PageSize / 2);
    static_assert(MaxMediumSize >= MaxSmallSize);
    static_assert(MaxMediumSize >= PageSize);
    static_assert(PageSize / Granularity <= 0xFFFE);
    static_assert(std::atomic<kheap_detail::free_node*>::is_always_lock_free);
    static_assert(std::atomic<void*>::is_always_lock_free);
    static_assert(std::atomic_ref<std::uintptr_t>::is_always_lock_free);

    static constexpr std::size_t MAX_REQUEST
        = (~std::size_t(0) >> 1) & ~(PageSize - 1);
    // Medium slab span cap; also the largest block offset + 1 in a slab.
    static constexpr std::uint32_t MAX_MEDIUM_SPAN = static_cast<std::uint32_t>(
        (MaxMediumSize * 2 + PageSize - 1) / PageSize
    );
    static constexpr std::uint32_t MEDIUM_TARGET_BLOCKS = 8;
    static constexpr std::uint32_t EXTEND_BATCH = 32;
    static constexpr unsigned OFFSET_BITS = static_cast<unsigned>(
        std::bit_width(std::size_t(MAX_MEDIUM_SPAN) * PageSize - 1)
    );

    static_assert(MaxMediumSize <= std::size_t(MAX_MEDIUM_SPAN) * PageSize);
    static_assert(MAX_MEDIUM_SPAN <= 0xFFFF);

    static constexpr auto specs
        = kheap_detail::make_bucket_table<sizes_t, NUM_BUCKETS, OFFSET_BITS>();

    // ---- pfndb word layout
    // ---------------------------------------------------
    static constexpr unsigned TAG_BITS = 5;
    static constexpr std::uint64_t TAG_MASK
        = (std::uint64_t(1) << TAG_BITS) - 1;
    static constexpr unsigned SLOT_BITS = PageShift - TAG_BITS;
    static constexpr std::uint64_t SLOT_MASK
        = (std::uint64_t(1) << SLOT_BITS) - 1;
    static constexpr std::uint64_t PAGE_MASK = ~(std::uint64_t(PageSize) - 1);
    static constexpr std::size_t META_SLOTS = PageSize / sizeof(kpage_meta);

    static_assert(META_SLOTS >= 1 && META_SLOTS - 1 <= SLOT_MASK);

    [[no_unique_address]] PAllocator palloc { };
    [[no_unique_address]] Accessor pfndb { };
    [[no_unique_address]] TRoutines troutines { };

    // Shared state. `shared_lock` guards the abandoned lists and orphaned
    // page_meta slots.
    Mutex shared_lock;
    kpage_meta* abandoned[NUM_BUCKETS] { };
    kpage_meta* meta_orphans { nullptr }; // slots of released caches
    std::atomic<void*> deferred { nullptr }; // large frees from atomic ctx

    // ---- helpers
    // -------------------------------------------------------------

    static constexpr std::size_t get_bucket_index(std::size_t size) noexcept
    {
        return sizes_t::bucket_index(size);
    }

    static constexpr std::size_t get_block_size(std::size_t b) noexcept
    {
        return sizes_t::block_size(b);
    }

    static constexpr bool is_medium_bucket(std::size_t b) noexcept
    {
        return b >= SMALL_BUCKETS;
    }

    // Span enough pages so medium slabs hold multiple blocks without tail
    // waste.
    static constexpr std::uint32_t medium_span_pages(
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

    static void report() noexcept
    {
        if constexpr (DebugChecks::enabled)
            DebugChecks::fail();
    }

    // Owner is read by remote frees while an adopter/abandoner writes it, so
    // all accesses go through atomic_ref (layout stays a plain uintptr_t).
    static std::uintptr_t load_owner(kpage_meta* pg) noexcept
    {
        return std::atomic_ref<std::uintptr_t>(pg->owner).load(
            std::memory_order_relaxed
        );
    }

    static void store_owner(kpage_meta* pg, std::uintptr_t v) noexcept
    {
        std::atomic_ref<std::uintptr_t>(pg->owner).store(
            v, std::memory_order_release
        );
    }

    // ---- memdesc encode / decode
    // ---------------------------------------------

    static std::uint64_t encode_slab_md(const kpage_meta* pg) noexcept
    {
        const auto a = reinterpret_cast<std::uint64_t>(pg);
        const std::uint64_t slot = (a & ~PAGE_MASK) / sizeof(kpage_meta);
        return (a & PAGE_MASK) | (slot << TAG_BITS) | TAG_SLAB;
    }

    static kpage_meta* decode_slab_md(std::uint64_t md) noexcept
    {
        const std::uint64_t slot = (md >> TAG_BITS) & SLOT_MASK;
        return reinterpret_cast<kpage_meta*>(
            (md & PAGE_MASK) + slot * sizeof(kpage_meta)
        );
    }

    std::uintptr_t pfn_of(const void* p) const noexcept
    {
        return pfndb.virt_to_pfn(p);
    }

    void stamp(std::uintptr_t pfn, std::uint64_t md) noexcept
    {
        pfndb[pfn] = desc_t::from_raw(md);
    }

    void reset_range(std::uintptr_t pfn, std::size_t count) noexcept
    {
        for (std::size_t i = 0; i < count; ++i)
            stamp(pfn + i, TAG_BUDDY);
    }

    void stamp_large(
        std::uintptr_t pfn,
        std::size_t first,
        std::size_t count
    ) noexcept
    {
        if (first == 0)
            stamp(pfn, (std::uint64_t(count) << TAG_BITS) | TAG_LARGE);
        for (std::size_t i = glox::max(first, std::size_t(1)); i < count; ++i)
            stamp(pfn + i, (std::uint64_t(i) << TAG_BITS) | TAG_LARGE_TAIL);
    }

    // slab block lists (16-bit indices)

    static std::uint16_t& link_of(void* blk) noexcept
    {
        return *static_cast<std::uint16_t*>(blk);
    }

    static char* block_at(const kpage_meta* pg, std::size_t idx) noexcept
    {
        return pg->start + idx * pg->block_size;
    }

    // ptr -> block index, without a hardware divide. False on interior ptrs.
    static bool slab_index(
        const kpage_meta* pg,
        const void* ptr,
        std::size_t& idx
    ) noexcept
    {
        const std::size_t off = static_cast<std::size_t>(
            static_cast<const char*>(ptr) - pg->start
        );
        const auto& sp = specs.spec[pg->bucket];
        idx = (off * sp.recip_mult) >> sp.recip_shift;
        return idx < pg->capacity && idx * pg->block_size == off;
    }

    static void push_chain(
        std::atomic<kheap_detail::free_node*>& head,
        kheap_detail::free_node* first,
        kheap_detail::free_node* last
    ) noexcept
    {
        kheap_detail::free_node* old = head.load(std::memory_order_relaxed);
        do {
            last->next = old;
        } while (!head.compare_exchange_weak(
            old, first, std::memory_order_release, std::memory_order_relaxed
        ));
    }

    // Free into a slab we do not own (or from atomic context). Touches only
    // the slab's atomic list; the block stays counted in `used` until the
    // owner drains it, so the slab (and its page_meta) cannot be retired
    // underneath us. This is the entire lifetime-pinning story: no RCU.
    static void remote_push(kpage_meta* pg, std::size_t idx, void* blk) noexcept
    {
        std::uint16_t old = pg->xthread_head.load(std::memory_order_relaxed);
        do {
            link_of(blk) = old;
        } while (!pg->xthread_head.compare_exchange_weak(
            old,
            static_cast<std::uint16_t>(idx),
            std::memory_order_release,
            std::memory_order_relaxed
        ));
    }

    // Owner only. Folds remote frees into local_free_head and fixes `used`.
    static void collect(kpage_meta* pg) noexcept
    {
        if (pg->xthread_head.load(std::memory_order_relaxed)
            != kpage_meta::EMPTY) {
            const std::uint16_t x = pg->xthread_head.exchange(
                kpage_meta::EMPTY, std::memory_order_acquire
            );
            if (x != kpage_meta::EMPTY) {
                std::uint32_t n = 1;
                std::uint16_t tail = x;
                for (;;) {
                    const std::uint16_t nx = link_of(block_at(pg, tail));
                    if (nx == kpage_meta::EMPTY)
                        break;
                    tail = nx;
                    ++n;
                }
                link_of(block_at(pg, tail)) = pg->local_free_head;
                pg->local_free_head = x;
                pg->used = static_cast<std::uint16_t>(pg->used - n);
            }
        }
        if (pg->free_head == kpage_meta::EMPTY
            && pg->local_free_head != kpage_meta::EMPTY) {
            pg->free_head = pg->local_free_head;
            pg->local_free_head = kpage_meta::EMPTY;
        }
    }

    // Lazily carve more blocks so a fresh slab does not touch all its memory.
    // For medium slabs, cap carving to ~PageSize bytes per batch so large
    // multi-page slabs don't fault untouched pages prematurely.
    static void extend(kpage_meta* pg) noexcept
    {
        const std::uint32_t remaining
            = static_cast<std::uint32_t>(pg->capacity - pg->reserved);
        if (remaining == 0)
            return;
        const std::uint32_t batch
            = pg->kind == kheap_detail::page_kind::medium
                ? glox::max(
                      std::uint32_t(1),
                      static_cast<std::uint32_t>(PageSize / pg->block_size)
                  )
                : EXTEND_BATCH;
        const std::uint32_t n = glox::min(batch, remaining);
        const std::uint32_t first = pg->reserved;
        for (std::uint32_t i = 0; i < n; ++i) {
            const std::uint32_t idx = first + i;
            link_of(block_at(pg, idx)) = (i + 1 < n)
                                           ? static_cast<std::uint16_t>(idx + 1)
                                           : kpage_meta::EMPTY;
        }
        pg->free_head = static_cast<std::uint16_t>(first);
        pg->reserved = static_cast<std::uint16_t>(first + n);
    }

    static bool has_room(const kpage_meta* pg) noexcept
    {
        return pg->free_head != kpage_meta::EMPTY
            || pg->reserved < pg->capacity;
    }

    static void* pop(kpage_meta* pg) noexcept
    {
        if (pg->free_head == kpage_meta::EMPTY)
            extend(pg);
        char* b = block_at(pg, pg->free_head);
        pg->free_head = link_of(b);
        pg->used++;
        return b;
    }

    // Debug (DebugChecks::deep): is `idx` carved and not already on one of the
    // owner's free lists?
    static bool owner_free_ok(const kpage_meta* pg, std::size_t idx) noexcept
    {
        if (idx >= pg->reserved)
            return false;
        const std::uint16_t heads[2] = { pg->free_head, pg->local_free_head };
        for (const std::uint16_t h : heads) {
            std::uint32_t guard = 0;
            for (std::uint16_t i = h;
                i != kpage_meta::EMPTY && guard <= pg->reserved;
                i = link_of(block_at(pg, i)), ++guard)
                if (i == idx)
                    return false;
        }
        return true;
    }

    // page_meta pool (owner-private; no atomics)
    void refill_meta(tcache_t& tc)
    {
        // Slots of destroyed caches first; takes the lock, but only once per
        // META_SLOTS slabs.
        shared_lock.lock();
        kpage_meta* orphans = meta_orphans;
        meta_orphans = nullptr;
        shared_lock.unlock();
        if (orphans) {
            tc.meta_free_list = orphans;
            return;
        }

        // Raw page straight from palloc: never routed through this heap.
        auto h = palloc.alloc(PageSize);
        if (!h.ptr)
            return;
        const std::uintptr_t pfn = pfn_of(h.ptr);
        if ((reinterpret_cast<std::uintptr_t>(h.ptr) & (PageSize - 1)) != 0
            || !pfndb.is_valid(pfn)) {
            palloc.dealloc(h.ptr, PageSize);
            return;
        }
        stamp(pfn, TAG_META);
        char* base = static_cast<char*>(h.ptr);
        kpage_meta* head = nullptr;
        for (std::size_t i = META_SLOTS; i-- > 0;) {
            auto* s = new (base + i * sizeof(kpage_meta)) kpage_meta();
            s->next = head;
            head = s;
        }
        tc.meta_free_list = head;
    }

    kpage_meta* alloc_meta_slot(tcache_t& tc)
    {
        if (!tc.meta_free_list)
            refill_meta(tc);
        kpage_meta* slot = tc.meta_free_list;
        if (!slot)
            return nullptr;
        tc.meta_free_list = slot->next;
        return new (slot) kpage_meta();
    }

    static void free_meta_slot(tcache_t& tc, kpage_meta* pg) noexcept
    {
        pg->next = tc.meta_free_list;
        tc.meta_free_list = pg;
    }

    // Return a slab's pages to palloc and its page_meta to the owner's pool.
    // pfndb is reset *before* palloc sees the pages (palloc may overwrite or
    // coalesce the head descriptor, and another CPU may reallocate at once).
    void retire_slab(tcache_t& tc, kpage_meta* pg)
    {
        char* start = pg->start;
        const std::size_t pages = pg->page_count;
        reset_range(pfn_of(start), pages);
        palloc.dealloc(start, pages << PageShift);
        free_meta_slot(tc, pg);
    }

    // large spans / deferred frees
    void free_large(void* ptr, std::uintptr_t pfn, std::uint64_t md)
    {
        const std::size_t pages = static_cast<std::size_t>(md >> TAG_BITS);
        reset_range(pfn, pages);
        palloc.dealloc(ptr, pages << PageShift);
    }

    void defer_large(void* ptr) noexcept
    {
        void* old = deferred.load(std::memory_order_relaxed);
        do {
            *static_cast<void**>(ptr) = old;
        } while (!deferred.compare_exchange_weak(
            old, ptr, std::memory_order_release, std::memory_order_relaxed
        ));
    }

    void drain_deferred()
    {
        if (deferred.load(std::memory_order_relaxed) == nullptr)
            return;
        void* p = deferred.exchange(nullptr, std::memory_order_acquire);
        while (p) {
            void* nx = *static_cast<void**>(p); // read first: p dies below
            const std::uintptr_t pfn = pfn_of(p);
            const std::uint64_t md = pfndb[pfn].raw();
            if ((md & TAG_MASK) == TAG_LARGE)
                free_large(p, pfn, md);
            p = nx;
        }
    }

    kpage_meta* take_abandoned(std::size_t b) noexcept
    {
        shared_lock.lock();
        kpage_meta* pg = abandoned[b];
        if (pg)
            abandoned[b] = pg->next;
        shared_lock.unlock();
        return pg;
    }

    void abandon(kpage_meta* pg, std::size_t b) noexcept
    {
        store_owner(pg, 0);
        shared_lock.lock();
        pg->next = abandoned[b];
        abandoned[b] = pg;
        shared_lock.unlock();
    }

    // allocation slow paths
    void* alloc_cached_slow(tcache_t& tc, std::size_t b)
    {
        drain_deferred();

        // 1. Existing slabs: collect remote frees, pick a slab with room,
        //    return surplus empty slabs.
        kpage_meta* cand = nullptr;
        kpage_meta** candLink = nullptr;
        kpage_meta** link = &tc.pages[b];
        while (kpage_meta* pg = *link) {
            collect(pg);
            if (pg->used == 0 && cand != nullptr) {
                *link = pg->next;
                retire_slab(tc, pg);
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

        // 2. Adopt slabs abandoned by other caches.
        const std::uintptr_t tok = token(tc);
        while (kpage_meta* pg = take_abandoned(b)) {
            store_owner(pg, tok);
            collect(pg);
            pg->next = tc.pages[b];
            tc.pages[b] = pg;
            if (has_room(pg))
                return pop(pg);
        }

        // 3. Fresh slab: a page_meta slot plus `span` pages from palloc.
        const bool medium = is_medium_bucket(b);
        const std::size_t blk_size = get_block_size(b);
        const std::uint32_t span = medium ? medium_span_pages(blk_size) : 1;

        kpage_meta* pg = alloc_meta_slot(tc);
        if (!pg)
            return nullptr;
        auto h = palloc.alloc(std::size_t(span) << PageShift);
        if (!h.ptr) {
            free_meta_slot(tc, pg); // roll back
            return nullptr;
        }
        const std::uintptr_t pfn = pfn_of(h.ptr);
        if ((reinterpret_cast<std::uintptr_t>(h.ptr) & (PageSize - 1)) != 0
            || !pfndb.is_valid(pfn) || !pfndb.is_valid(pfn + span - 1)) {
            palloc.dealloc(h.ptr, std::size_t(span) << PageShift);
            free_meta_slot(tc, pg);
            return nullptr;
        }

        pg->kind = medium ? kheap_detail::page_kind::medium
                          : kheap_detail::page_kind::small;
        pg->bucket = static_cast<std::uint16_t>(b);
        pg->start = static_cast<char*>(h.ptr);
        pg->block_size = static_cast<std::uint32_t>(blk_size);
        pg->page_count = static_cast<std::uint16_t>(span);
        pg->capacity = static_cast<std::uint16_t>(
            (std::size_t(span) << PageShift) / blk_size
        );
        store_owner(pg, tok);

        const std::uint64_t md = encode_slab_md(pg);
        for (std::uint32_t i = 0; i < span; ++i)
            stamp(pfn + i, md);

        pg->next = tc.pages[b];
        tc.pages[b] = pg;
        return pop(pg);
    }

    alloc_handle<void> alloc_large(std::size_t n)
    {
        drain_deferred();
        const std::size_t k = n == 0 ? 1 : (n + PageSize - 1) >> PageShift;
        auto h = palloc.alloc(k << PageShift);
        if (!h.ptr)
            return { nullptr, 0 };
        const std::uintptr_t pfn = pfn_of(h.ptr);
        if ((reinterpret_cast<std::uintptr_t>(h.ptr) & (PageSize - 1)) != 0
            || !pfndb.is_valid(pfn) || !pfndb.is_valid(pfn + k - 1)) {
            palloc.dealloc(h.ptr, k << PageShift);
            return { nullptr, 0 };
        }
        stamp_large(pfn, 0, k);
        return { h.ptr, k << PageShift };
    }

    // In-place resize of a large span through palloc.grow/shrink. Returns the
    // new usable bytes, or 0 if the span could not (or need not) be resized.
    std::size_t resize_large_inplace(void* ptr, std::size_t new_size)
    {
        if ((reinterpret_cast<std::uintptr_t>(ptr) & (PageSize - 1)) != 0)
            return 0;
        const std::uintptr_t pfn = pfn_of(ptr);
        if (!pfndb.is_valid(pfn))
            return 0;
        const std::uint64_t md = pfndb[pfn].raw();
        if ((md & TAG_MASK) != TAG_LARGE)
            return 0;
        const std::size_t old_pages = static_cast<std::size_t>(md >> TAG_BITS);
        const std::size_t new_pages
            = glox::max((new_size + PageSize - 1) >> PageShift, std::size_t(1));
        if (new_pages == old_pages)
            return old_pages << PageShift;

        if (new_pages > old_pages) {
            auto h = palloc.grow(
                ptr, old_pages << PageShift, new_pages << PageShift
            );
            if (h.ptr != ptr)
                return 0;
            stamp_large(pfn, 0, new_pages); // head count + new tails
            return new_pages << PageShift;
        }

        // Shrink: release the tail descriptors first, restore on failure.
        reset_range(pfn + new_pages, old_pages - new_pages);
        auto h = palloc.shrink(
            ptr, old_pages << PageShift, new_pages << PageShift
        );
        if (h.ptr != ptr) {
            for (std::size_t i = new_pages; i < old_pages; ++i)
                stamp(pfn + i, (std::uint64_t(i) << TAG_BITS) | TAG_LARGE_TAIL);
            return 0;
        }
        stamp(pfn, (std::uint64_t(new_pages) << TAG_BITS) | TAG_LARGE);
        return new_pages << PageShift;
    }

    void dealloc_slab(void* ptr, kpage_meta* pg)
    {
        std::size_t idx = 0;
        if (!slab_index(pg, ptr, idx)) { // interior pointer
            report();
            return;
        }
        auto& tc = troutines.get_tcache();
        if (load_owner(pg) != token(tc)) {
            remote_push(pg, idx, ptr);
            return;
        }
        if (pg->used == 0 || idx >= pg->reserved) {
            report();
            return;
        }
        if constexpr (DebugChecks::deep) {
            if (!owner_free_ok(pg, idx)) { // double free
                report();
                return;
            }
        }
        link_of(ptr) = pg->local_free_head;
        pg->local_free_head = static_cast<std::uint16_t>(idx);
        pg->used--;

        // Keep the active head medium slab cached for reuse hysteresis, but
        // return completely empty non-head medium slabs promptly.
        if (pg->kind == kheap_detail::page_kind::medium && pg->used == 0
            && tc.pages[pg->bucket] != pg) {
            kpage_meta** link = &tc.pages[pg->bucket];
            while (*link && *link != pg)
                link = &(*link)->next;
            if (*link == pg) {
                *link = pg->next;
                retire_slab(tc, pg);
            }
        }
    }

public:
    kheap_allocator() = default;

    explicit kheap_allocator(
        PAllocator pa,
        Accessor acc = Accessor(),
        TRoutines tr = TRoutines()
    )
        : palloc(static_cast<PAllocator&&>(pa))
        , pfndb(static_cast<Accessor&&>(acc))
        , troutines(static_cast<TRoutines&&>(tr))
    {
    }

    kheap_allocator(const kheap_allocator&) = delete;
    kheap_allocator& operator=(const kheap_allocator&) = delete;

    [[nodiscard]] PAllocator& page_allocator_ref() noexcept
    {
        return palloc;
    }
    [[nodiscard]] Accessor& accessor() noexcept
    {
        return pfndb;
    }
    [[nodiscard]] TRoutines& routines() noexcept
    {
        return troutines;
    }

    // introspection (pure, constexpr; used by tests)
    static constexpr std::size_t bucket_of(std::size_t size) noexcept
    {
        return get_bucket_index(size);
    }
    static constexpr std::size_t block_size_of(std::size_t b) noexcept
    {
        return get_block_size(b);
    }
    static constexpr std::size_t recip_index(
        std::size_t b,
        std::size_t off
    ) noexcept
    {
        return (off * specs.spec[b].recip_mult) >> specs.spec[b].recip_shift;
    }
    static constexpr std::size_t max_slab_bytes() noexcept
    {
        return std::size_t(MAX_MEDIUM_SPAN) * PageSize;
    }
    static constexpr std::size_t meta_slots_per_page() noexcept
    {
        return META_SLOTS;
    }
    static std::uint64_t encode_slab(const kpage_meta* pg) noexcept
    {
        return encode_slab_md(pg);
    }
    static kpage_meta* decode_slab(std::uint64_t md) noexcept
    {
        return decode_slab_md(md);
    }

    // allocation
    alloc_handle<void> alloc(std::size_t n, std::size_t alignment = Granularity)
    {
        std::size_t alignedN = 0;
        if (!normalize(n, alignment, alignedN))
            return { nullptr, 0 };
        if (!is_cached(alignedN))
            return alloc_large(n);

        const std::size_t b = get_bucket_index(alignedN);
        auto& tc = troutines.get_tcache();

        // Fast path: entirely slab-local, no atomics, no locks.
        if (kpage_meta* pg = tc.pages[b]) {
            const std::uint16_t i = pg->free_head;
            if (i != kpage_meta::EMPTY) {
                char* blk = block_at(pg, i);
                pg->free_head = link_of(blk);
                pg->used++;
                return { blk, pg->block_size };
            }
        }
        void* p = alloc_cached_slow(tc, b);
        if (!p)
            return { nullptr, 0 };
        return { p, specs.spec[b].block_size };
    }

    std::size_t usable_size(void* ptr) noexcept
    {
        if (!ptr)
            return 0;
        const std::uintptr_t pfn = pfn_of(ptr);
        if (!pfndb.is_valid(pfn)) {
            report();
            return 0;
        }
        const std::uint64_t md = pfndb[pfn].raw();
        const std::size_t off
            = reinterpret_cast<std::uintptr_t>(ptr) & (PageSize - 1);
        switch (md & TAG_MASK) {
        case TAG_SLAB:
            return decode_slab_md(md)->block_size;
        case TAG_LARGE:
            return (static_cast<std::size_t>(md >> TAG_BITS) << PageShift)
                 - off;
        case TAG_LARGE_TAIL: {
            const std::uint64_t i = md >> TAG_BITS;
            const std::uint64_t head = pfndb[pfn - i].raw();
            if ((head & TAG_MASK) != TAG_LARGE) {
                report();
                return 0;
            }
            return (static_cast<std::size_t>((head >> TAG_BITS) - i)
                       << PageShift)
                 - off;
        }
        default:
            report();
            return 0;
        }
    }

    void dealloc(void* ptr)
    {
        if (!ptr)
            return;
        const std::uintptr_t pfn = pfn_of(ptr);
        if (!pfndb.is_valid(pfn)) { // foreign pointer
            report();
            return;
        }
        const std::uint64_t md = pfndb[pfn].raw();
        switch (md & TAG_MASK) {
        case TAG_SLAB:
            dealloc_slab(ptr, decode_slab_md(md));
            return;
        case TAG_LARGE:
            if ((reinterpret_cast<std::uintptr_t>(ptr) & (PageSize - 1)) != 0) {
                report();
                return;
            }
            drain_deferred();
            free_large(ptr, pfn, md);
            return;
        default:
            // TAG_BUDDY: double free / never allocated. TAG_META and
            // TAG_LARGE_TAIL are not allocation bases either.
            report();
            return;
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

    // interrupt-safe path
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
            auto h = alloc(n, alignment); // counted as in use: pins its slab
            if (!h.ptr)
                break;
            auto* node = static_cast<kheap_detail::free_node*>(h.ptr);
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
        kheap_detail::free_node* all
            = stash.exchange(nullptr, std::memory_order_acquire);
        if (!all)
            return { nullptr, 0 };
        if (kheap_detail::free_node* rest = all->next) {
            glox::kheap_detail::free_node* last = rest;
            while (last->next)
                last = last->next;
            push_chain(stash, rest, last);
        }
        return { all, specs.spec[b].block_size };
    }

    void dealloc_atomic(void* ptr) noexcept
    {
        if (!ptr)
            return;
        const std::uintptr_t pfn = pfn_of(ptr);
        if (!pfndb.is_valid(pfn)) {
            report();
            return;
        }
        const std::uint64_t md = pfndb[pfn].raw();
        switch (md & TAG_MASK) {
        case TAG_SLAB: {
            kpage_meta* pg = decode_slab_md(md);
            std::size_t idx = 0;
            if (!slab_index(pg, ptr, idx)) {
                report();
                return;
            }
            remote_push(pg, idx, ptr); // owner folds it in during collect()
            return;
        }
        case TAG_LARGE:
            if ((reinterpret_cast<std::uintptr_t>(ptr) & (PageSize - 1)) != 0) {
                report();
                return;
            }
            // Released on the next normal-context alloc/free.
            defer_large(ptr);
            return;
        default:
            report();
            return;
        }
    }

    // Call when a thread/CPU cache goes away. Empty slabs return to palloc;
    // the rest are abandoned so another cache can adopt them.
    void release_cache(tcache_t& tc)
    {
        for (std::size_t b = 0; b < NUM_BUCKETS; ++b) {
            glox::kheap_detail::free_node* s = tc.atomic_stash[b].exchange(
                nullptr, std::memory_order_acq_rel
            );
            while (s) {
                kheap_detail::free_node* nx = s->next;
                dealloc_atomic(s);
                s = nx;
            }
        }
        for (std::size_t b = 0; b < NUM_BUCKETS; ++b) {
            kpage_meta* pg = tc.pages[b];
            tc.pages[b] = nullptr;
            while (pg) {
                kpage_meta* nx = pg->next;
                collect(pg);
                if (pg->used == 0)
                    retire_slab(tc, pg);
                else
                    abandon(pg, b);
                pg = nx;
            }
        }
        // The cache is going away: hand its page_meta slots to the others.
        if (kpage_meta* head = tc.meta_free_list) {
            kpage_meta* last = head;
            while (last->next)
                last = last->next;
            tc.meta_free_list = nullptr;
            shared_lock.lock();
            last->next = meta_orphans;
            meta_orphans = head;
            shared_lock.unlock();
        }
    }

    // resizing
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

        // Large spans first try to grow in place through palloc; the
        // descriptors (and the user pointer) stay put.
        if (new_size <= MAX_REQUEST) {
            if (const std::size_t got = resize_large_inplace(ptr, new_size))
                return { ptr, got };
        }

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

        const std::uint64_t md = pfndb[pfn_of(ptr)].raw();
        switch (md & TAG_MASK) {
        case TAG_LARGE:
            if (const std::size_t got = resize_large_inplace(ptr, new_size))
                return { ptr, got };
            break;
        case TAG_SLAB:
            // Medium: move to a smaller bucket when reclaiming >= 1 page.
            if (decode_slab_md(md)->kind == kheap_detail::page_kind::medium
                && cur - new_size >= PageSize) {
                auto nh = alloc(new_size, alignment);
                if (nh.ptr) {
                    glox::memcpy(nh.ptr, ptr, new_size);
                    dealloc(ptr);
                    return nh;
                }
            }
            break;
        default:
            break;
        }
        // Small blocks: not worth moving to a smaller bucket.
        return { ptr, cur };
    }
};

} // namespace glox
