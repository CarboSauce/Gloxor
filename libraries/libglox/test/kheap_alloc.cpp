// Hosted doctest suite for glox::heap_allocator (pfndb/memdesc design).
//
// Fixtures:
//   * radix_db<Shift>   sparse 3-level radix tree keyed by pfn (= vaddr >>
//   Shift),
//                       covers arbitrary 48-bit virtual addresses
//   * test_pfndb<Shift> pfndb_accessor handle onto that tree
//   * page_fixture      posix_memalign-backed page allocator with registry,
//                       OOM injection and "descriptor must be TAG_BUDDY when
//                       handed back" verification
//   * cache_set/test_routines  per-"vcpu" thread caches (vcpu_scope switches
//   the
//                       logical CPU inside one thread, so remote frees,
//                       abandon and adopt are testable deterministically)
#include "doctest/doctest.h"

#include "glox/kheap_alloc.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

// pfndb: descriptor + sparse radix tree + accessor
struct test_desc
{
    std::uint64_t v { 0 };

    // Buddy-side encoding lives entirely under tag 0: bit 5 = free, [63:6] =
    // order. Anything with a non-zero tag is therefore "not buddy free".
    static test_desc make_reserved() noexcept
    {
        return { 0 };
    }
    static test_desc make_buddy_free(std::size_t o) noexcept
    {
        return { (std::uint64_t(o) << 6) | 32 };
    }
    static test_desc make_allocated(std::size_t o) noexcept
    {
        return { std::uint64_t(o) << 6 };
    }
    bool is_buddy_free() const noexcept
    {
        return (v & 31) == 0 && (v & 32) != 0;
    }
    std::size_t buddy_order() const noexcept
    {
        return v >> 6;
    }

    static test_desc from_raw(std::uint64_t r) noexcept
    {
        return { r };
    }
    std::uint64_t raw() const noexcept
    {
        return v;
    }
};

template <unsigned Shift>
struct radix_db
{
    static constexpr std::size_t N = 4096; // 12 bits per level, 3 levels
    struct leaf
    {
        test_desc e[N];
    };
    struct mid
    {
        std::atomic<leaf*> c[N];
    };
    struct top
    {
        std::atomic<mid*> c[N];
    };

    static top& root()
    {
        static top t { };
        return t;
    }
    static std::mutex& mtx()
    {
        static std::mutex m;
        return m;
    }

    static test_desc* find(std::uintptr_t pfn) noexcept
    {
        if ((pfn >> 24) >= N)
            return nullptr;
        mid* m = root().c[pfn >> 24].load(std::memory_order_acquire);
        if (!m)
            return nullptr;
        leaf* l = m->c[(pfn >> 12) & (N - 1)].load(std::memory_order_acquire);
        if (!l)
            return nullptr;
        return &l->e[pfn & (N - 1)];
    }

    static void ensure(std::uintptr_t first, std::size_t count)
    {
        std::lock_guard g(mtx());
        for (std::size_t i = 0; i < count;) {
            const std::uintptr_t pfn = first + i;
            auto& slot = root().c[pfn >> 24];
            mid* m = slot.load(std::memory_order_acquire);
            if (!m) {
                m = new mid { };
                slot.store(m, std::memory_order_release);
            }
            auto& lslot = m->c[(pfn >> 12) & (N - 1)];
            if (!lslot.load(std::memory_order_acquire))
                lslot.store(new leaf { }, std::memory_order_release);
            i += N - (pfn & (N - 1));
        }
    }
};

template <unsigned Shift>
struct test_pfndb
{
    test_desc& operator[](std::uintptr_t pfn)
    {
        auto* p = radix_db<Shift>::find(pfn);
        if (p)
            return *p;
        static thread_local test_desc dummy;
        return dummy;
    }
    const test_desc& operator[](std::uintptr_t pfn) const
    {
        auto* p = radix_db<Shift>::find(pfn);
        if (p)
            return *p;
        static const test_desc zero { };
        return zero;
    }
    bool is_valid(std::uintptr_t pfn) const
    {
        return radix_db<Shift>::find(pfn) != nullptr;
    }
    void* pfn_to_virt(std::uintptr_t pfn) const
    {
        return reinterpret_cast<void*>(pfn << Shift);
    }
    std::uintptr_t virt_to_pfn(const void* p) const
    {
        return reinterpret_cast<std::uintptr_t>(p) >> Shift;
    }
    static constexpr std::uint64_t TAG_BUDDY = 0x00;
    static constexpr std::uint64_t TAG_META = 0x01;
    static constexpr std::uint64_t TAG_SLAB = 0x02;
    static constexpr std::uint64_t TAG_LARGE = 0x03;
    static constexpr std::uint64_t TAG_LARGE_TAIL = 0x04;
};

// page allocator fixture
struct page_fixture
{
    struct block
    {
        std::size_t bytes;
        std::size_t cap_pages; // pow2, like a buddy; grow/shrink stay inside
    };

    std::mutex m;
    std::map<std::uintptr_t, block> live;
    std::size_t budget = ~std::size_t(0);
    std::size_t used_bytes = 0;
    std::size_t alloc_calls = 0, dealloc_calls = 0;
    std::size_t grow_calls = 0, shrink_calls = 0;
    std::size_t fail_at = 0; // 1-based alloc call number to fail (0 = never)
    int dirty = 0; // descriptors not TAG_BUDDY at alloc/dealloc/shrink
    int bad_size = 0; // dealloc/grow/shrink with a size we did not record
    int bad_ptr = 0; // unknown pointer handed back (double palloc free)
    void (*cleanup)(page_fixture&) = nullptr;

    page_fixture() = default;
    page_fixture(const page_fixture&) = delete;

    ~page_fixture()
    {
        if (cleanup)
            cleanup(*this);
    }

    bool clean() const
    {
        return dirty == 0 && bad_size == 0 && bad_ptr == 0;
    }

    template <unsigned S>
    static void do_cleanup(page_fixture& f)
    {
        for (auto& [addr, b] : f.live) {
            for (std::size_t i = 0; i < b.cap_pages; ++i)
                if (auto* d = radix_db<S>::find((addr >> S) + i))
                    d->v = 0;
            std::free(reinterpret_cast<void*>(addr));
        }
        f.live.clear();
    }

    template <unsigned S>
    glox::alloc_handle<void> do_alloc(std::size_t n)
    {
        std::lock_guard g(m);
        cleanup = &do_cleanup<S>;
        ++alloc_calls;
        if (fail_at != 0 && alloc_calls == fail_at)
            return { nullptr, 0 };
        if (n == 0 || (n & ((std::size_t(1) << S) - 1)) != 0) {
            ++bad_size;
            return { nullptr, 0 };
        }
        if (n > budget - used_bytes || n > (std::size_t(1) << 30))
            return { nullptr, 0 }; // OOM / absurd request
        const std::size_t pages = n >> S;
        const std::size_t cap = std::bit_ceil(pages);
        void* p = nullptr;
        if (posix_memalign(&p, std::size_t(1) << S, cap << S) != 0)
            return { nullptr, 0 };
        const auto pfn = reinterpret_cast<std::uintptr_t>(p) >> S;
        radix_db<S>::ensure(pfn, cap);
        for (std::size_t i = 0; i < cap; ++i)
            if (radix_db<S>::find(pfn + i)->v != 0)
                ++dirty;
        std::memset(p, 0xA5, n); // poison: nobody may assume zeroed pages
        live[reinterpret_cast<std::uintptr_t>(p)] = { n, cap };
        used_bytes += n;
        return { p, n };
    }

    template <unsigned S>
    void do_dealloc(void* p, std::size_t n)
    {
        std::lock_guard g(m);
        ++dealloc_calls;
        auto it = live.find(reinterpret_cast<std::uintptr_t>(p));
        if (it == live.end()) {
            ++bad_ptr;
            return;
        }
        if (it->second.bytes != n)
            ++bad_size;
        const auto pfn = it->first >> S;
        for (std::size_t i = 0; i < it->second.cap_pages; ++i)
            if (radix_db<S>::find(pfn + i)->v != 0)
                ++dirty; // heap forgot to reset a pfndb entry to TAG_BUDDY
        used_bytes -= it->second.bytes;
        live.erase(it);
        std::free(p);
    }

    template <unsigned S>
    glox::alloc_handle<void> do_grow(void* p, std::size_t o, std::size_t n)
    {
        std::lock_guard g(m);
        ++grow_calls;
        auto it = live.find(reinterpret_cast<std::uintptr_t>(p));
        if (it == live.end()) {
            ++bad_ptr;
            return { nullptr, 0 };
        }
        if (it->second.bytes != o)
            ++bad_size;
        if (n > (it->second.cap_pages << S) || n - o > budget - used_bytes)
            return { nullptr, 0 };
        std::memset(static_cast<char*>(p) + o, 0xA5, n - o);
        used_bytes += n - o;
        it->second.bytes = n;
        return { p, n };
    }

    template <unsigned S>
    glox::alloc_handle<void> do_shrink(void* p, std::size_t o, std::size_t n)
    {
        std::lock_guard g(m);
        ++shrink_calls;
        auto it = live.find(reinterpret_cast<std::uintptr_t>(p));
        if (it == live.end()) {
            ++bad_ptr;
            return { nullptr, 0 };
        }
        if (it->second.bytes != o)
            ++bad_size;
        const auto pfn = it->first >> S;
        for (std::size_t i = n >> S; i < (o >> S); ++i)
            if (radix_db<S>::find(pfn + i)->v != 0)
                ++dirty; // tail descriptors must be reset before shrink
        used_bytes -= o - n;
        it->second.bytes = n;
        return { p, n };
    }
};

template <unsigned S>
struct test_palloc
{
    page_fixture* fx { nullptr };
    glox::alloc_handle<void> alloc(std::size_t n)
    {
        return fx->template do_alloc<S>(n);
    }
    void dealloc(void* p, std::size_t n)
    {
        fx->template do_dealloc<S>(p, n);
    }
    glox::alloc_handle<void> grow(void* p, std::size_t o, std::size_t n)
    {
        return fx->template do_grow<S>(p, o, n);
    }
    glox::alloc_handle<void> shrink(void* p, std::size_t o, std::size_t n)
    {
        return fx->template do_shrink<S>(p, o, n);
    }
};

// per-vcpu thread caches
inline std::atomic<int> g_next_vcpu { 1 };
inline thread_local int tl_vcpu = 0;
inline int current_vcpu()
{
    if (tl_vcpu == 0)
        tl_vcpu = g_next_vcpu++;
    return tl_vcpu;
}
struct vcpu_scope
{
    int old;
    explicit vcpu_scope(int id)
        : old(tl_vcpu)
    {
        tl_vcpu = id;
    }
    ~vcpu_scope()
    {
        tl_vcpu = old;
    }
};

template <std::size_t N>
struct cache_set
{
    std::mutex m;
    std::map<int, std::unique_ptr<glox::kthread_cache<N>>> caches;

    glox::kthread_cache<N>& get(int id)
    {
        std::lock_guard g(m);
        auto& p = caches[id];
        if (!p)
            p = std::make_unique<glox::kthread_cache<N>>();
        return *p;
    }
    template <typename F>
    void for_each(F f)
    {
        std::vector<glox::kthread_cache<N>*> v;
        {
            std::lock_guard g(m);
            for (auto& [id, c] : caches)
                v.push_back(c.get());
        }
        for (auto* c : v)
            f(*c);
    }
};

template <std::size_t N>
struct test_routines
{
    cache_set<N>* set { nullptr };
    glox::kthread_cache<N>& get_tcache()
    {
        return set->get(current_vcpu());
    }
};

// Counts reports instead of trapping; double-free walks are enabled.
struct test_checks
{
    static constexpr bool enabled = true;
    static constexpr bool deep = true;
    static inline std::atomic<int> reports { 0 };
    static void fail() noexcept
    {
        ++reports;
    }
};

template <
    unsigned PS = 12,
    std::size_t MaxSmall = 2048,
    std::size_t Gran = 8,
    std::size_t MaxMed = 65536,
    typename Policy = test_checks
>
struct basic_env
{
    static constexpr unsigned SHIFT = PS;
    static constexpr std::size_t PAGE = std::size_t(1) << PS;
    static constexpr std::size_t NB
        = glox::kheap_detail::total_bucket_count<MaxSmall, Gran, MaxMed>();
    using routines_t = test_routines<NB>;
    using heap_t = glox::kheap_allocator<
        test_palloc<PS>,
        test_pfndb<PS>,
        routines_t,
        glox::spinlock,
        PS,
        MaxSmall,
        Gran,
        MaxMed,
        Policy
    >;

    page_fixture fx;
    cache_set<NB> caches;
    heap_t heap;

    basic_env()
        : heap(
              test_palloc<PS> { &fx },
              test_pfndb<PS> { },
              routines_t { &caches }
          )
    {
    }

    glox::kthread_cache<NB>& cache()
    {
        return caches.get(current_vcpu());
    }
    void release_all()
    {
        caches.for_each([&](auto& c) { heap.release_cache(c); });
    }

    // Live palloc blocks whose head descriptor has (equal) / lacks tag.
    std::size_t count_live(std::uint64_t tag, bool equal)
    {
        std::lock_guard g(fx.m);
        std::size_t n = 0;
        for (auto& [addr, blk] : fx.live) {
            auto* d = radix_db<PS>::find(addr >> PS);
            const bool is = d && (d->v & 31) == tag;
            if (is == equal)
                ++n;
        }
        return n;
    }
    std::size_t non_meta_live()
    {
        return count_live(heap_t::TAG_META, false);
    }
    std::size_t meta_pages()
    {
        return count_live(heap_t::TAG_META, true);
    }

    // Raw descriptor word of the page containing p.
    std::uint64_t md_of(const void* p)
    {
        return radix_db<PS>::find(reinterpret_cast<std::uintptr_t>(p) >> PS)->v;
    }
    // palloc block [base, base+bytes) holding p.
    page_fixture::block block_of(const void* p, std::uintptr_t* base = nullptr)
    {
        std::lock_guard g(fx.m);
        auto it = fx.live.upper_bound(reinterpret_cast<std::uintptr_t>(p));
        --it;
        if (base)
            *base = it->first;
        return it->second;
    }
    bool clean()
    {
        return fx.clean();
    }
};

using env = basic_env<>;
using noenv = basic_env<12, 2048, 8, 65536, glox::kheap_detail::no_checks>;

static void fill(void* p, std::size_t n, unsigned char v)
{
    std::memset(p, v, n);
}
static bool verify(const void* p, std::size_t n, unsigned char v)
{
    auto* c = static_cast<const unsigned char*>(p);
    for (std::size_t i = 0; i < n; ++i)
        if (c[i] != v)
            return false;
    return true;
}
static std::uintptr_t addr(const void* p)
{
    return reinterpret_cast<std::uintptr_t>(p);
}

// compile-time contracts
static_assert(glox::pfndb_accessor<test_pfndb<12>>);
static_assert(glox::kheap_detail::memdesc_accessor<test_pfndb<12>>);
static_assert(glox::kheap_detail::page_allocator<test_palloc<12>>);
static_assert(sizeof(glox::kpage_meta) == 48);
static_assert(alignof(glox::kpage_meta) == 8);
static_assert(std::atomic<std::uint16_t>::is_always_lock_free);
// Every shipped debug policy instantiates.
static_assert(
    sizeof(
        basic_env<12, 2048, 8, 65536, glox::kheap_detail::trap_checks>::heap_t
    )
    > 0
);
static_assert(
    sizeof(basic_env<12, 2048, 8, 65536, glox::kheap_detail::deep_trap_checks>::
            heap_t)
    > 0
);

// size classes / reciprocal / encoding
TEST_CASE("size classes: monotone, covering, bounded waste")
{
    using H = env::heap_t;
    std::size_t prev = 0;
    for (std::size_t n = 1; n <= 65536; ++n) {
        const std::size_t b = H::bucket_of(n);
        const std::size_t bs = H::block_size_of(b);
        REQUIRE(b < env::NB);
        REQUIRE(b >= prev);
        REQUIRE(bs >= n);
        REQUIRE(bs % 8 == 0);
        if (n <= 2048)
            REQUIRE(bs - n < 8);
        else
            REQUIRE(bs * 4 <= n * 5); // < 25% internal fragmentation
        prev = b;
    }
    for (std::size_t b = 0; b < env::NB; ++b)
        REQUIRE_EQ(H::bucket_of(H::block_size_of(b)), b);
    CHECK_EQ(H::bucket_of(65536), env::NB - 1);
    CHECK_EQ(H::block_size_of(0), 8u);
    CHECK_EQ(H::block_size_of(255), 2048u);
    CHECK_EQ(H::block_size_of(256), 2560u);
}

TEST_CASE("reciprocal multiply-shift equals integer division (exhaustive)")
{
    using H = env::heap_t;
    for (std::size_t b = 0; b < env::NB; ++b) {
        const std::size_t bs = H::block_size_of(b);
        for (std::size_t off = 0; off < H::max_slab_bytes(); ++off)
            REQUIRE_EQ(H::recip_index(b, off), off / bs);
    }
}

TEST_CASE("memdesc encoding keeps every bit above PageShift (LA57)")
{
    using H = env::heap_t;
    CHECK_EQ(H::meta_slots_per_page(), 85u);
    const std::uint64_t bases[] = {
        0x0000'7fff'dead'b000ull, // 48-bit user
        0xffff'8880'1234'5000ull, // 48-bit kernel
        0xff00'1234'5678'9000ull, // 57-bit kernel, bit 56 set
        0xfe00'0000'0000'1000ull, // 57-bit kernel
        0xffff'ffff'ffff'f000ull,
    };
    for (std::uint64_t base : bases)
        for (std::uint64_t slot : { 0ull, 1ull, 42ull, 84ull }) {
            auto* pg = reinterpret_cast<glox::kpage_meta*>(base + slot * 48);
            const std::uint64_t md = H::encode_slab(pg);
            CHECK_EQ(md & 31, H::TAG_SLAB);
            CHECK_EQ((md >> 5) & 127, slot);
            CHECK_EQ(md & ~0xFFFull, base);
            CHECK(H::decode_slab(md) == pg);
        }
}

// small / medium / large
TEST_CASE("small: every size, no overlap, pattern integrity, no leak")
{
    env e;
    std::map<std::uintptr_t, std::size_t> ranges;
    std::vector<std::pair<void*, std::size_t>> v;
    for (std::size_t n = 1; n <= 2048; ++n) {
        auto h = e.heap.alloc(n);
        REQUIRE(h.ptr != nullptr);
        CHECK_EQ(addr(h.ptr) % 8, 0u);
        CHECK_GE(h.count, n);
        CHECK_EQ(h.count, e.heap.usable_size(h.ptr));
        fill(h.ptr, h.count, static_cast<unsigned char>(n));
        ranges[addr(h.ptr)] = h.count;
        v.push_back({ h.ptr, n });
    }
    std::uintptr_t prev_end = 0;
    for (auto& [a, s] : ranges) {
        CHECK_GE(a, prev_end);
        prev_end = a + s;
    }
    for (auto& [p, n] : v) {
        CHECK(verify(p, e.heap.usable_size(p), static_cast<unsigned char>(n)));
        e.heap.dealloc(p, n);
    }
    e.release_all();
    CHECK_EQ(e.non_meta_live(), 0u);
    CHECK(e.clean());
}

TEST_CASE("zero in-page headers: first block sits at byte 0 of the span")
{
    env e;
    auto h = e.heap.alloc(64);
    std::uintptr_t base = 0;
    e.block_of(h.ptr, &base);
    CHECK_EQ(addr(h.ptr), base);
    // A multi-page medium slab as well.
    auto m = e.heap.alloc(30000);
    e.block_of(m.ptr, &base);
    CHECK_EQ(addr(m.ptr), base);
}

TEST_CASE("medium: sizes, multi-page slabs stamp every PFN identically")
{
    env e;
    using H = env::heap_t;
    for (std::size_t n : { 2049u, 3000u, 4096u, 8192u, 30000u, 65536u }) {
        std::vector<void*> v;
        for (int i = 0; i < 20; ++i) {
            auto h = e.heap.alloc(n);
            REQUIRE(h.ptr != nullptr);
            CHECK_GE(h.count, n);
            fill(h.ptr, n, static_cast<unsigned char>(i + 1));
            v.push_back(h.ptr);
        }
        for (int i = 0; i < 20; ++i)
            CHECK(verify(v[i], n, static_cast<unsigned char>(i + 1)));
        for (void* p : v) {
            std::uintptr_t base = 0;
            auto blk = e.block_of(p, &base);
            const std::uint64_t md = e.md_of(p);
            REQUIRE_EQ(md & 31, H::TAG_SLAB);
            glox::kpage_meta* pg = H::decode_slab(md);
            CHECK_EQ(addr(pg->start), base);
            CHECK_EQ(std::size_t(pg->page_count) << 12, blk.bytes);
            for (std::size_t i = 0; i < pg->page_count; ++i)
                CHECK_EQ(e.md_of(pg->start + (i << 12)), md);
        }
        for (void* p : v)
            e.heap.dealloc(p);
    }
    e.release_all();
    CHECK_EQ(e.non_meta_live(), 0u);
    CHECK(e.clean());
}

TEST_CASE("medium: empty non-head slabs return to palloc promptly")
{
    env e;
    std::vector<void*> v;
    for (int i = 0; i < 16; ++i) // 4096-byte blocks, 8 per slab -> 2 slabs
        v.push_back(e.heap.alloc(4096).ptr);
    CHECK_EQ(e.non_meta_live(), 2u);
    const auto d0 = e.fx.dealloc_calls;
    for (int i = 0; i < 8; ++i) // first slab (older, non-head)
        e.heap.dealloc(v[i]);
    CHECK_EQ(e.fx.dealloc_calls, d0 + 1);
    for (int i = 8; i < 16; ++i) // head slab stays cached
        e.heap.dealloc(v[i]);
    CHECK_EQ(e.fx.dealloc_calls, d0 + 1);
    CHECK_EQ(e.non_meta_live(), 1u);
    e.release_all();
    CHECK_EQ(e.non_meta_live(), 0u);
    CHECK(e.clean());
}

TEST_CASE("large: spans, tail descriptors, interior usable_size, exact free")
{
    env e;
    using H = env::heap_t;
    for (std::size_t n :
        { 65537u, 70000u, 131072u, 1u << 20, (3u << 20) + 1 }) {
        auto h = e.heap.alloc(n);
        REQUIRE(h.ptr != nullptr);
        const std::size_t pages = (n + 4095) / 4096;
        CHECK_EQ(addr(h.ptr) % 4096, 0u);
        CHECK_EQ(h.count, pages * 4096);
        CHECK_EQ(e.heap.usable_size(h.ptr), pages * 4096);
        const std::uint64_t head = e.md_of(h.ptr);
        CHECK_EQ(head & 31, H::TAG_LARGE);
        CHECK_EQ(head >> 5, pages);
        for (std::size_t i = 1; i < pages; ++i) {
            const std::uint64_t t
                = e.md_of(static_cast<char*>(h.ptr) + i * 4096);
            REQUIRE_EQ(t & 31, H::TAG_LARGE_TAIL);
            REQUIRE_EQ(t >> 5, i);
        }
        for (std::size_t i : { 0u, 100u, 4096u, 4096u * 3 + 7u })
            if (i < h.count)
                CHECK_EQ(
                    e.heap.usable_size(static_cast<char*>(h.ptr) + i),
                    h.count - i
                );
        fill(h.ptr, h.count, 0x3C);
        CHECK(verify(h.ptr, h.count, 0x3C));
        e.heap.dealloc(h.ptr, n);
        CHECK_EQ(e.md_of(h.ptr), H::TAG_BUDDY);
    }
    CHECK_EQ(e.non_meta_live(), 0u);
    CHECK(e.clean());
}

TEST_CASE("alignment edge cases")
{
    env e;
    const std::size_t sizes[] = {
        0, 1, 7, 8, 9, 63, 64, 100, 1000, 2048, 2049, 4096, 5000, 65536, 70000
    };
    for (std::size_t al = 8; al <= 4096; al <<= 1)
        for (std::size_t n : sizes) {
            std::vector<void*> v;
            for (int i = 0; i < 70; ++i) {
                auto h = e.heap.alloc(n, al);
                REQUIRE(h.ptr != nullptr);
                REQUIRE_EQ(addr(h.ptr) % al, 0u);
                REQUIRE_GE(h.count, n);
                v.push_back(h.ptr);
            }
            for (void* p : v)
                e.heap.dealloc(p, n, al);
        }
    CHECK(e.heap.alloc(64, 8192).ptr == nullptr); // > PageSize
    CHECK(e.heap.alloc(64, 3).ptr == nullptr); // not a power of two
    CHECK(e.heap.alloc(64, 0).ptr == nullptr);
    CHECK(e.heap.alloc(~std::size_t(0)).ptr == nullptr);
    CHECK(e.heap.alloc((~std::size_t(0) >> 1) + 1).ptr == nullptr);
    CHECK(
        e.heap.alloc(~std::size_t(0) >> 1).ptr == nullptr
    ); // OOM, no overflow
    auto z = e.heap.alloc(0);
    CHECK(z.ptr != nullptr);
    e.heap.dealloc(z.ptr);
    e.release_all();
    CHECK_EQ(e.non_meta_live(), 0u);
    CHECK(e.clean());
}

// metadata lifecycle
TEST_CASE("page_meta comes from raw palloc pages, never from the heap")
{
    env e;
    std::vector<void*> v;
    for (int i = 0; i < 512 * 100;
        ++i) // 8-byte blocks: 512 per slab -> 100 slabs
        v.push_back(e.heap.alloc(8).ptr);
    CHECK_EQ(e.non_meta_live(), 100u);
    CHECK_EQ(e.meta_pages(), 2u); // ceil(100 / 85)
    CHECK_EQ(e.fx.alloc_calls, 102u); // 100 slabs + 2 meta pages, nothing else
    {
        std::lock_guard g(e.fx.m);
        for (auto& [a, b] : e.fx.live)
            if ((radix_db<12>::find(a >> 12)->v & 31) == env::heap_t::TAG_META)
                CHECK_EQ(b.bytes, 4096u);
    }
    for (void* p : v)
        e.heap.dealloc(p);
    e.release_all();
    CHECK_EQ(e.non_meta_live(), 0u);
    CHECK_EQ(e.meta_pages(), 2u); // type-stable: meta pages are never returned
    CHECK(e.clean());
}

TEST_CASE("release_cache hands its page_meta slots to other caches")
{
    env e;
    {
        vcpu_scope s(1001);
        void* p = e.heap.alloc(32).ptr;
        e.heap.dealloc(p);
        e.heap.release_cache(e.cache());
    }
    CHECK_EQ(e.fx.alloc_calls, 2u); // meta page + slab
    {
        vcpu_scope s(1002);
        void* p = e.heap.alloc(32).ptr;
        CHECK_EQ(e.fx.alloc_calls, 3u); // slab only: slot adopted, no new meta
        e.heap.dealloc(p);
        e.heap.release_cache(e.cache());
    }
    CHECK_EQ(e.meta_pages(), 1u);
    CHECK_EQ(e.non_meta_live(), 0u);
    CHECK(e.clean());
}

// remote frees, lifetime pinning, abandon / adopt
TEST_CASE("remote frees pin the slab until the owner collects")
{
    env e;
    std::vector<void*> v;
    {
        vcpu_scope s(1001);
        for (int i = 0; i < 3; ++i)
            v.push_back(e.heap.alloc(64).ptr);
    }
    {
        vcpu_scope s(1002); // remote CPU: only pushes onto xthread_head
        for (void* p : v)
            e.heap.dealloc(p);
    }
    CHECK_EQ(e.fx.dealloc_calls, 0u); // nobody retired anything
    CHECK_EQ(e.non_meta_live(), 1u);
    {
        vcpu_scope s(1001);
        auto before = e.fx.alloc_calls;
        for (int i = 0; i < 3; ++i) { // reuse without touching palloc
            void* p = e.heap.alloc(64).ptr;
            CHECK(p != nullptr);
            e.heap.dealloc(p);
        }
        CHECK_EQ(e.fx.alloc_calls, before);
    }
    e.release_all();
    CHECK_EQ(e.non_meta_live(), 0u);
    CHECK(e.clean());
}

TEST_CASE("cross-thread remote frees")
{
    env e;
    constexpr int N = 400;
    std::vector<void*> blocks;
    {
        vcpu_scope s(1001);
        for (int i = 0; i < N; ++i) {
            auto h = e.heap.alloc(64);
            fill(h.ptr, 64, static_cast<unsigned char>(i));
            blocks.push_back(h.ptr);
        }
    }
    for (int i = 0; i < N; ++i)
        CHECK(verify(blocks[i], 64, static_cast<unsigned char>(i)));
    std::thread t([&] {
        vcpu_scope s(1002);
        for (void* p : blocks)
            e.heap.dealloc(p);
    });
    t.join();
    CHECK_EQ(e.fx.dealloc_calls, 0u);
    e.release_all();
    CHECK_EQ(e.non_meta_live(), 0u);
    CHECK(e.clean());
}

TEST_CASE("concurrent producer/consumer: owner allocs while remote frees")
{
    env e;
    constexpr int N = 50000;
    std::mutex m;
    std::vector<void*> q;
    std::atomic<bool> done { false };
    std::thread prod([&] {
        vcpu_scope s(1001);
        for (int i = 0; i < N; ++i) {
            auto h = e.heap.alloc(8 + (i % 300));
            REQUIRE(h.ptr != nullptr);
            *static_cast<std::uint64_t*>(h.ptr)
                = (0xC0FFEEull << 32) | std::uint64_t(i);
            std::lock_guard g(m);
            q.push_back(h.ptr);
        }
        done = true;
    });
    std::thread cons([&] {
        vcpu_scope s(1002);
        std::size_t freed = 0;
        while (freed < N) {
            std::vector<void*> take;
            {
                std::lock_guard g(m);
                take.swap(q);
            }
            for (void* p : take) {
                CHECK_EQ(*static_cast<std::uint64_t*>(p) >> 32, 0xC0FFEEu);
                e.heap.dealloc(p);
                ++freed;
            }
        }
    });
    prod.join();
    cons.join();
    e.release_all();
    CHECK_EQ(e.non_meta_live(), 0u);
    CHECK(e.clean());
}

TEST_CASE("abandon / adopt")
{
    env e;
    std::vector<void*> v;
    {
        vcpu_scope s(1001);
        for (int i = 0; i < 10; ++i) {
            auto h = e.heap.alloc(48);
            fill(h.ptr, 48, static_cast<unsigned char>(0x40 + i));
            v.push_back(h.ptr);
        }
        e.heap.release_cache(e.cache()); // live blocks -> slab abandoned
    }
    CHECK_EQ(e.non_meta_live(), 1u);
    {
        vcpu_scope s(1002);
        const auto calls = e.fx.alloc_calls;
        auto h = e.heap.alloc(48); // adopts the abandoned slab
        CHECK_EQ(e.fx.alloc_calls, calls);
        CHECK(h.ptr != nullptr);
        for (int i = 0; i < 10; ++i)
            CHECK(verify(v[i], 48, static_cast<unsigned char>(0x40 + i)));
        // Old blocks are now owned by cpu 1002 -> local frees.
        for (int i = 0; i < 5; ++i)
            e.heap.dealloc(v[i]);
        e.heap.dealloc(h.ptr);
    }
    {
        vcpu_scope s(1003); // third CPU frees the rest remotely
        for (int i = 5; i < 10; ++i)
            e.heap.dealloc(v[i]);
    }
    e.release_all();
    CHECK_EQ(e.non_meta_live(), 0u);
    CHECK(e.clean());
}

TEST_CASE("remote free into an abandoned slab is picked up by the adopter")
{
    env e;
    void* p;
    {
        vcpu_scope s(1001);
        p = e.heap.alloc(48).ptr;
        e.heap.release_cache(e.cache());
    }
    {
        vcpu_scope s(1002);
        e.heap.dealloc(p); // owner == 0 -> remote push
    }
    {
        vcpu_scope s(1003);
        const auto calls = e.fx.alloc_calls;
        void* q = e.heap.alloc(48).ptr; // adopt + collect
        CHECK_EQ(e.fx.alloc_calls, calls);
        e.heap.dealloc(q);
    }
    e.release_all();
    CHECK_EQ(e.non_meta_live(), 0u);
    CHECK(e.clean());
}

// grow / shrink
TEST_CASE("grow: null, zero, in-block, bucket change, chain to large")
{
    env e;
    {
        auto a = e.heap.grow(nullptr, 100); // == alloc
        CHECK(a.ptr != nullptr);
        e.heap.dealloc(a.ptr);
    }
    {
        auto h = e.heap.grow(nullptr, 100);
        auto z = e.heap.grow(h.ptr, 0);
        CHECK(z.ptr == nullptr);
    }
    auto h = e.heap.alloc(10); // 16-byte block
    CHECK_EQ(h.count, 16u);
    fill(h.ptr, 10, 0x11);
    auto g = e.heap.grow(h.ptr, 16);
    CHECK(g.ptr == h.ptr); // fits the block
    CHECK_EQ(g.count, 16u);
    g = e.heap.grow(h.ptr, 17); // new bucket: moves
    CHECK(g.ptr != nullptr);
    CHECK(verify(g.ptr, 10, 0x11));
    fill(g.ptr, g.count, 0x22);
    for (std::size_t n : { 5000u, 70000u, 400000u }) {
        auto ng = e.heap.grow(g.ptr, n);
        REQUIRE(ng.ptr != nullptr);
        CHECK_GE(ng.count, n);
        CHECK(verify(ng.ptr, 16, 0x22));
        fill(ng.ptr, ng.count, 0x22);
        g = ng;
    }
    e.heap.dealloc(g.ptr);
    e.release_all();
    CHECK_EQ(e.non_meta_live(), 0u);
    CHECK(e.clean());
}

TEST_CASE("grow/shrink of a large span happens in place through palloc")
{
    env e;
    using H = env::heap_t;
    auto h = e.heap.alloc(17 * 4096); // 17 pages, palloc cap = 32 pages
    REQUIRE(h.ptr != nullptr);
    fill(h.ptr, h.count, 0x77);

    auto g = e.heap.grow(h.ptr, 30 * 4096); // inside the cap: in place
    CHECK(g.ptr == h.ptr);
    CHECK_EQ(g.count, 30u * 4096);
    CHECK_EQ(e.md_of(h.ptr) >> 5, 30u);
    CHECK_EQ(
        e.md_of(static_cast<char*>(h.ptr) + 29 * 4096) & 31, H::TAG_LARGE_TAIL
    );
    CHECK(verify(h.ptr, 17 * 4096, 0x77));
    CHECK_EQ(e.fx.grow_calls, 1u);

    g = e.heap.grow(h.ptr, 33 * 4096); // beyond the cap: palloc refuses -> move
    REQUIRE(g.ptr != nullptr);
    CHECK(g.ptr != h.ptr);
    CHECK(verify(g.ptr, 17 * 4096, 0x77));
    CHECK_EQ(e.md_of(h.ptr), H::TAG_BUDDY); // old span fully released

    auto s = e.heap.shrink(g.ptr, 18 * 4096); // in place
    CHECK(s.ptr == g.ptr);
    CHECK_EQ(s.count, 18u * 4096);
    CHECK_EQ(e.md_of(g.ptr) >> 5, 18u);
    CHECK_EQ(e.md_of(static_cast<char*>(g.ptr) + 18 * 4096), H::TAG_BUDDY);
    CHECK_EQ(e.md_of(static_cast<char*>(g.ptr) + 32 * 4096), H::TAG_BUDDY);
    CHECK(verify(g.ptr, 17 * 4096, 0x77));

    s = e.heap.shrink(g.ptr, 100); // stays a (one page) large span
    CHECK(s.ptr == g.ptr);
    CHECK_EQ(s.count, 4096u);
    CHECK_EQ(e.md_of(g.ptr) >> 5, 1u);
    e.heap.dealloc(g.ptr);
    CHECK_EQ(e.non_meta_live(), 0u);
    CHECK(e.clean());
}

TEST_CASE("shrink: medium moves when reclaiming a page, small stays")
{
    env e;
    auto m = e.heap.alloc(40000);
    fill(m.ptr, 40000, 0x5A);
    auto s = e.heap.shrink(m.ptr, m.count - 100); // < one page
    CHECK(s.ptr == m.ptr);
    s = e.heap.shrink(
        m.ptr, 1000
    ); // reclaims >= a page: moves to a small block
    REQUIRE(s.ptr != nullptr);
    CHECK(s.ptr != m.ptr);
    CHECK_EQ(s.count, 1000u);
    CHECK(verify(s.ptr, 1000, 0x5A));

    auto sm = e.heap.alloc(100);
    auto s2 = e.heap.shrink(sm.ptr, 10);
    CHECK(s2.ptr == sm.ptr);
    CHECK_EQ(s2.count, sm.count);
    CHECK(e.heap.shrink(nullptr, 10).ptr == nullptr);
    CHECK(e.heap.shrink(sm.ptr, 0).ptr == nullptr); // frees
    e.heap.dealloc(s.ptr);
    e.release_all();
    CHECK_EQ(e.non_meta_live(), 0u);
    CHECK(e.clean());
}

TEST_CASE("grow failure leaves the original block intact")
{
    env e;
    auto h = e.heap.alloc(100);
    fill(h.ptr, 100, 0x99);
    e.fx.budget = e.fx.used_bytes; // no more pages
    auto g = e.heap.grow(h.ptr, 500000);
    CHECK(g.ptr == nullptr);
    CHECK(verify(h.ptr, 100, 0x99));
    e.heap.dealloc(h.ptr);
    e.release_all();
    CHECK_EQ(e.non_meta_live(), 0u);
    CHECK(e.clean());
}

TEST_CASE("grow_inplace / shrink_inplace")
{
    env e;
    auto h = e.heap.alloc(100); // usable 104
    CHECK(e.heap.grow_inplace(h.ptr, 104) == h.ptr);
    CHECK(e.heap.grow_inplace(h.ptr, 105) == nullptr);
    CHECK(e.heap.shrink_inplace(h.ptr, 8) == h.ptr);
    CHECK(e.heap.grow_inplace(nullptr, 8) == nullptr);
    e.heap.dealloc(h.ptr);
    e.release_all();
}

// interrupt-safe path
TEST_CASE("alloc_atomic / dealloc_atomic on slab blocks")
{
    env e;
    CHECK(e.heap.alloc_atomic(64).ptr == nullptr); // nothing staged
    CHECK_EQ(e.heap.reserve_atomic(100000, 4), 0u); // large is not stageable
    CHECK_EQ(e.heap.reserve_atomic(64, 10), 10u);
    std::vector<void*> v;
    for (int i = 0; i < 10; ++i) {
        auto h = e.heap.alloc_atomic(64);
        REQUIRE(h.ptr != nullptr);
        CHECK_EQ(h.count, 64u);
        v.push_back(h.ptr);
    }
    CHECK(e.heap.alloc_atomic(64).ptr == nullptr); // 11th: stash empty
    for (void* p : v)
        e.heap.dealloc_atomic(p); // remote push; owner folds in on collect
    void* q = e.heap.alloc(64).ptr; // slow path collects
    CHECK(q != nullptr);
    e.heap.dealloc(q);
    e.release_all();
    CHECK_EQ(e.non_meta_live(), 0u);
    CHECK(e.clean());
}

TEST_CASE("reserve_atomic leftovers are flushed by release_cache")
{
    env e;
    CHECK_EQ(e.heap.reserve_atomic(128, 5), 5u);
    e.release_all();
    CHECK_EQ(e.non_meta_live(), 0u);
    CHECK(e.clean());
}

TEST_CASE("dealloc_atomic on a large span defers until normal context")
{
    env e;
    void* a = e.heap.alloc(100000).ptr;
    void* b = e.heap.alloc(200000).ptr;
    e.heap.dealloc_atomic(a);
    e.heap.dealloc_atomic(b);
    CHECK_EQ(e.fx.dealloc_calls, 0u); // atomic context never calls palloc
    CHECK_EQ(e.non_meta_live(), 2u);
    void* c = e.heap.alloc(70000).ptr; // normal context drains the list
    CHECK_EQ(e.fx.dealloc_calls, 2u);
    CHECK_EQ(e.non_meta_live(), 1u);
    e.heap.dealloc(c);
    CHECK_EQ(e.non_meta_live(), 0u);
    CHECK(e.clean());
}

// OOM
TEST_CASE("OOM: page_meta slot is rolled back when the span allocation fails")
{
    env e;
    e.fx.fail_at = 2; // call 1 = meta page, call 2 = the slab span
    CHECK(e.heap.alloc(64).ptr == nullptr);
    CHECK_EQ(e.fx.alloc_calls, 2u);
    CHECK_EQ(e.non_meta_live(), 0u);
    e.fx.fail_at = 0;
    void* p = e.heap.alloc(64).ptr;
    CHECK(p != nullptr);
    CHECK_EQ(e.fx.alloc_calls, 3u); // slab only: the slot was recycled
    CHECK_EQ(e.meta_pages(), 1u);
    e.heap.dealloc(p);
    e.release_all();
    CHECK(e.clean());
}

TEST_CASE("OOM: meta page, medium, large, budget exhaustion")
{
    env e;
    e.fx.fail_at = 1; // the meta page itself
    CHECK(e.heap.alloc(64).ptr == nullptr);
    e.fx.fail_at = 0;
    e.fx.budget = 0;
    CHECK(e.heap.alloc(64).ptr == nullptr);
    CHECK(e.heap.alloc(30000).ptr == nullptr);
    CHECK(e.heap.alloc(1 << 20).ptr == nullptr);
    CHECK_EQ(e.non_meta_live(), 0u);
    e.fx.budget = ~std::size_t(0);
    void* p = e.heap.alloc(64).ptr;
    CHECK(p != nullptr);
    e.heap.dealloc(p);
    e.release_all();
    CHECK_EQ(e.non_meta_live(), 0u);
    CHECK(e.clean());
}

// misuse
TEST_CASE("double free of a large span traps once and frees once")
{
    env e;
    void* p = e.heap.alloc(100000).ptr;
    e.heap.dealloc(p);
    const int r = test_checks::reports;
    e.heap.dealloc(p); // TAG_BUDDY now
    CHECK_EQ(test_checks::reports, r + 1);
    CHECK_EQ(e.fx.bad_ptr, 0); // the second free never reached palloc
    CHECK_EQ(e.fx.dealloc_calls, 1u);
    CHECK(e.clean());
}

TEST_CASE("double free of a slab block traps (deep) and is dropped")
{
    env e;
    void* p = e.heap.alloc(32).ptr;
    void* q = e.heap.alloc(32).ptr;
    e.heap.dealloc(p);
    const int r = test_checks::reports;
    e.heap.dealloc(p);
    CHECK_EQ(test_checks::reports, r + 1);
    e.heap.dealloc(q);
    e.release_all();
    CHECK_EQ(e.non_meta_live(), 0u); // used count was not corrupted
    CHECK(e.clean());
}

TEST_CASE("interior pointers trap and leave the allocation alive")
{
    env e;
    int r = test_checks::reports;
    void* s = e.heap.alloc(64).ptr;
    e.heap.dealloc(static_cast<char*>(s) + 8);
    CHECK_EQ(test_checks::reports, ++r);
    void* m = e.heap.alloc(30000).ptr;
    e.heap.dealloc(static_cast<char*>(m) + 8);
    CHECK_EQ(test_checks::reports, ++r);
    void* l = e.heap.alloc(100000).ptr;
    e.heap.dealloc(static_cast<char*>(l) + 8); // unaligned inside the head page
    CHECK_EQ(test_checks::reports, ++r);
    e.heap.dealloc(static_cast<char*>(l) + 4096); // TAG_LARGE_TAIL
    CHECK_EQ(test_checks::reports, ++r);
    e.heap.dealloc_atomic(static_cast<char*>(s) + 8);
    CHECK_EQ(test_checks::reports, ++r);
    CHECK_EQ(e.non_meta_live(), 3u); // nothing was freed
    e.heap.dealloc(s);
    e.heap.dealloc(m);
    e.heap.dealloc(l);
    e.release_all();
    CHECK_EQ(e.non_meta_live(), 0u);
    CHECK_EQ(test_checks::reports, r);
    CHECK(e.clean());
}

TEST_CASE("foreign, meta and never-allocated pointers trap")
{
    env e;
    void* s = e.heap.alloc(64).ptr;
    int r = test_checks::reports;
    int on_stack = 0;
    e.heap.dealloc(&on_stack);
    CHECK_EQ(test_checks::reports, ++r);
    CHECK_EQ(e.heap.usable_size(&on_stack), 0u);
    CHECK_EQ(test_checks::reports, ++r);
    void* meta = nullptr; // find the TAG_META page
    {
        std::lock_guard g(e.fx.m);
        for (auto& [a, b] : e.fx.live)
            if ((radix_db<12>::find(a >> 12)->v & 31) == env::heap_t::TAG_META)
                meta = reinterpret_cast<void*>(a);
    }
    REQUIRE(meta != nullptr);
    e.heap.dealloc(meta);
    CHECK_EQ(test_checks::reports, ++r);
    e.heap.dealloc(nullptr); // legal no-op
    CHECK_EQ(test_checks::reports, r);
    e.heap.dealloc(s);
    e.release_all();
    CHECK(e.clean());
}

TEST_CASE("without checks, misuse is silently ignored (no trap, no corruption)")
{
    noenv e;
    void* p = e.heap.alloc(100000).ptr;
    e.heap.dealloc(p);
    e.heap.dealloc(p);
    void* s = e.heap.alloc(64).ptr;
    e.heap.dealloc(static_cast<char*>(s) + 8);
    e.heap.dealloc(s);
    e.heap.dealloc(s);
    e.release_all();
    CHECK_EQ(e.non_meta_live(), 0u);
    CHECK(e.clean());
}

// other configurations
TEST_CASE("PageShift = 13")
{
    basic_env<13> e;
    using H = basic_env<13>::heap_t;
    CHECK_EQ(H::meta_slots_per_page(), 170u);
    for (std::size_t al = 8; al <= 8192; al <<= 1)
        for (std::size_t n : { 1u, 100u, 3000u, 20000u, 70000u, 300000u }) {
            void* p = e.heap.alloc(n, al).ptr;
            REQUIRE(p != nullptr);
            CHECK_EQ(addr(p) % al, 0u);
            fill(p, n, 0x6B);
            e.heap.dealloc(p);
        }
    void* l = e.heap.alloc(100000).ptr;
    CHECK_EQ(addr(l) % 8192, 0u);
    CHECK_EQ(e.heap.usable_size(l), 13u * 8192);
    e.heap.dealloc(l);
    e.release_all();
    CHECK_EQ(e.non_meta_live(), 0u);
    CHECK(e.clean());
}

TEST_CASE("alternative size-class configuration")
{
    basic_env<12, 1024, 16, 16384> e;
    std::vector<void*> v;
    for (std::size_t n = 1; n <= 16384; n += 37) {
        auto h = e.heap.alloc(n);
        REQUIRE(h.ptr != nullptr);
        CHECK_EQ(addr(h.ptr) % 16, 0u);
        fill(h.ptr, n, 0x2D);
        v.push_back(h.ptr);
    }
    for (void* p : v)
        CHECK_GE(e.heap.usable_size(p), 1u);
    for (void* p : v)
        e.heap.dealloc(p);
    e.release_all();
    CHECK_EQ(e.non_meta_live(), 0u);
    CHECK(e.clean());
}

// stress
TEST_CASE("stress: 4 threads, shared slot pool, cross-thread frees")
{
    env e;
    constexpr int T = 4, ITERS = 30000, SLOTS = 512;
    std::vector<std::atomic<void*>> pool(SLOTS);
    std::atomic<int> corrupt { 0 }, oom { 0 };

    auto check_and_free = [&](void* p) {
        std::uint64_t n;
        std::memcpy(&n, p, 8);
        const auto* c = static_cast<const unsigned char*>(p);
        for (std::uint64_t i = 8; i < n; ++i)
            if (c[i] != static_cast<unsigned char>(n)) {
                ++corrupt;
                break;
            }
        e.heap.dealloc(p);
    };

    auto worker = [&](int id) {
        vcpu_scope s(2000 + id);
        std::mt19937 rng(id * 7919 + 1);
        for (int it = 0; it < ITERS; ++it) {
            const std::size_t slot = rng() % SLOTS;
            void* old = pool[slot].exchange(nullptr, std::memory_order_acq_rel);
            if (old) {
                check_and_free(old);
                continue;
            }
            const unsigned r = rng() % 100;
            std::size_t n = r < 70 ? 8 + rng() % 505
                          : r < 95 ? 513 + rng() % 7680
                          : r < 99 ? 8193 + rng() % 57344
                                   : 65537 + rng() % 235000;
            auto h = e.heap.alloc(n);
            if (!h.ptr) {
                ++oom;
                continue;
            }
            std::memcpy(h.ptr, &n, 8);
            std::memset(
                static_cast<char*>(h.ptr) + 8,
                static_cast<unsigned char>(n),
                n - 8
            );
            void* expected = nullptr;
            if (!pool[slot].compare_exchange_strong(
                    expected, h.ptr, std::memory_order_acq_rel
                ))
                e.heap.dealloc(h.ptr);
        }
    };
    std::vector<std::thread> ts;
    for (int t = 0; t < T; ++t)
        ts.emplace_back(worker, t);
    for (auto& t : ts)
        t.join();

    for (auto& s : pool)
        if (void* p = s.exchange(nullptr))
            check_and_free(p); // main thread: remote frees into worker caches
    e.release_all();
    CHECK_EQ(corrupt.load(), 0);
    CHECK_EQ(oom.load(), 0);
    CHECK_EQ(e.non_meta_live(), 0u);
    CHECK(e.clean());
}

TEST_CASE("stress: interrupt-safe stash consumed concurrently (nested IRQs)")
{
    env e;
    constexpr int STAGED = 2000;
    {
        vcpu_scope s(1001);
        CHECK_EQ(e.heap.reserve_atomic(96, STAGED), std::size_t(STAGED));
    }
    std::mutex m;
    std::vector<void*> got;
    auto irq = [&] {
        vcpu_scope s(1001); // all "interrupt contexts" run on the staging CPU
        std::vector<void*> mine;
        for (int i = 0; i < STAGED; ++i)
            if (auto h = e.heap.alloc_atomic(96); h.ptr)
                mine.push_back(h.ptr);
        std::lock_guard g(m);
        got.insert(got.end(), mine.begin(), mine.end());
    };
    std::vector<std::thread> ts;
    for (int t = 0; t < 4; ++t)
        ts.emplace_back(irq);
    for (auto& t : ts)
        t.join();
    std::vector<void*> sorted = got;
    std::sort(sorted.begin(), sorted.end());
    CHECK(
        std::adjacent_find(sorted.begin(), sorted.end()) == sorted.end()
    ); // no dup
    CHECK_LE(got.size(), std::size_t(STAGED));
    for (void* p : got) {
        fill(p, 96, 0x1F);
        e.heap.dealloc_atomic(p);
    }
    e.release_all(); // flushes whatever is still staged
    CHECK_EQ(e.non_meta_live(), 0u);
    CHECK(e.clean());
}
