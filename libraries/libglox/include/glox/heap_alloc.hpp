#pragma once

#ifndef USE_MODULES
#include "alloc_types.hpp"
#include "utilalgs.hpp"
#include <algorithm>
#include <atomic>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
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

struct page_header;

// Intrusive list of pages per bucket
struct page_header
{
    std::atomic<free_node*> cross_thread_free { nullptr };
    std::uint32_t used_count { 0 }; // Active allocations in this page
    std::uint32_t total_blocks { 0 }; // Capacity of this page
    std::size_t owning_thread_id { 0 };
    std::size_t bucket_idx { 0 };
    std::size_t block_size { 0 };
    page_header* next_page { nullptr }; // Linked list of pages in bucket
};

// Thread/Core Local Cache
template <std::size_t NumBuckets>
struct thread_cache
{
    free_node* local_free[NumBuckets] { };
    page_header* pages[NumBuckets] { }; // Head of page list for each bucket
};

template <typename T, std::size_t NumBuckets>
concept thread_routines = requires(T thread) {
    { thread.thread_id() } -> std::same_as<std::size_t>;
    { thread.get_tcache() } -> std::same_as<thread_cache<NumBuckets>&>;
};

template <
    page_allocator PAllocator,
    typename TRoutines,
    std::size_t MaxSmallSize = 2048,
    std::size_t Granularity = 8,
    std::size_t PageSize = 65536
>
    requires thread_routines<TRoutines, MaxSmallSize / Granularity>
class heap_allocator
{
    static constexpr std::size_t NUM_BUCKETS = MaxSmallSize / Granularity;

    [[no_unique_address]] PAllocator palloc;
    [[no_unique_address]] TRoutines troutines;

    static constexpr std::size_t
    align_up(std::size_t value, std::size_t alignment) noexcept
    {
        return (value + alignment - 1) & ~(alignment - 1);
    }

    static constexpr std::size_t MAX_SMALL_ALIGNMENT
        = std::bit_ceil(MaxSmallSize);

    static constexpr std::size_t get_bucket_index(std::size_t size) noexcept
    {
        if (size == 0)
            return 0;
        return (size - 1) / Granularity;
    }

    static constexpr std::size_t get_block_size(std::size_t bucket_idx) noexcept
    {
        return (bucket_idx + 1) * Granularity;
    }

    static page_header* get_page_header(void* ptr) noexcept
    {
        return reinterpret_cast<page_header*>(
            reinterpret_cast<std::uintptr_t>(ptr) & ~(PageSize - 1)
        );
    }

    // Process cross-thread frees across all pages in the bucket and recycle
    // empty pages
    void
    collect_and_sweep(thread_cache<NUM_BUCKETS>& tcache, std::size_t bucket_idx)
    {
        page_header** prev_ptr = &tcache.pages[bucket_idx];
        page_header* curr = tcache.pages[bucket_idx];

        while (curr != nullptr) {
            // 1. Atomically steal cross-thread frees for this page
            if (curr->cross_thread_free.load(std::memory_order_relaxed)
                != nullptr) {
                free_node* stolen = curr->cross_thread_free.exchange(
                    nullptr, std::memory_order_acquire
                );

                std::uint32_t freed_count = 0;
                free_node* tail = stolen;
                while (tail != nullptr) {
                    freed_count++;
                    if (tail->next == nullptr)
                        break;
                    tail = tail->next;
                }

                if (stolen != nullptr) {
                    // Prepend stolen nodes to local freelist
                    tail->next = tcache.local_free[bucket_idx];
                    tcache.local_free[bucket_idx] = stolen;
                    curr->used_count -= freed_count;
                }
            }

            // 2. Garbage Collect: If page is completely empty and not the
            // active front page, return it to palloc
            page_header* next = curr->next_page;
            if (curr->used_count == 0 && curr != tcache.pages[bucket_idx]) {
                *prev_ptr = next; // Unlink page from list
                palloc.dealloc(curr, PageSize);
            } else {
                prev_ptr = &curr->next_page;
            }

            curr = next;
        }
    }

    void* allocate_slow_path(
        thread_cache<NUM_BUCKETS>& tcache,
        std::size_t bucket_idx,
        std::size_t tid
    )
    {
        // 1. Reclaim cross-thread frees and recycle empty pages in this bucket
        collect_and_sweep(tcache, bucket_idx);

        // If sweeping reclaimed blocks into local_free, serve allocation
        // immediately
        if (tcache.local_free[bucket_idx] != nullptr) {
            free_node* block = tcache.local_free[bucket_idx];
            tcache.local_free[bucket_idx] = block->next;

            page_header* page = get_page_header(block);
            page->used_count++;
            return block;
        }

        // 2. Fetch new page slab from page allocator
        auto handle = palloc.alloc(PageSize);
        if (handle.ptr == nullptr) {
            return nullptr;
        }

        auto* page = static_cast<page_header*>(handle.ptr);
        page->cross_thread_free.store(nullptr, std::memory_order_relaxed);
        page->owning_thread_id = tid;
        page->bucket_idx = bucket_idx;
        page->block_size = get_block_size(bucket_idx);
        page->used_count = 1; // First block is returned immediately

        // Link new page to head of bucket's page list
        page->next_page = tcache.pages[bucket_idx];
        tcache.pages[bucket_idx] = page;

        std::size_t offset = align_up(sizeof(page_header), MAX_SMALL_ALIGNMENT);
        void* firstBlock = static_cast<char*>(handle.ptr) + offset;
        offset += page->block_size;

        // 3. Carve remaining blocks into thread-local freelist (FIFO)
        free_node* head = nullptr;
        free_node** tailPtr = &head;
        std::uint32_t total = 1;

        while (offset + page->block_size <= PageSize) {
            auto* node = reinterpret_cast<free_node*>(
                static_cast<char*>(handle.ptr) + offset
            );
            *tailPtr = node;
            tailPtr = &node->next;
            offset += page->block_size;
            total++;
        }
        *tailPtr = nullptr;

        page->total_blocks = total;
        tcache.local_free[bucket_idx] = head;

        return firstBlock;
    }

public:
    heap_allocator() = default;

    alloc_handle<void> alloc(std::size_t n, std::size_t alignment = Granularity)
    {
        if (!std::has_single_bit(alignment) || alignment > PageSize) {
            return { nullptr, 0 };
        }

        alignment = std::max(alignment, Granularity);

        std::size_t alignedN = align_up(n, alignment);
        alignedN = align_up(alignedN, Granularity);

        if (alignedN > MaxSmallSize || alignment > MAX_SMALL_ALIGNMENT) {
            std::size_t pageReq = align_up(n, PageSize);
            return palloc.alloc(pageReq);
        }

        std::size_t bucketIdx = get_bucket_index(alignedN);
        auto& tcache = troutines.get_tcache();
        free_node* block = tcache.local_free[bucketIdx];

        // Fast path: thread-local freelist hit
        if (block != nullptr) {
            tcache.local_free[bucketIdx] = block->next;

            page_header* page = get_page_header(block);
            page->used_count++;

            return { block, get_block_size(bucketIdx) };
        }

        // Slow path: collect cross-thread frees or allocate new page slab
        void* ptr
            = allocate_slow_path(tcache, bucketIdx, troutines.thread_id());
        if (ptr == nullptr) {
            return { nullptr, 0 };
        }

        return { ptr, get_block_size(bucketIdx) };
    }

    void dealloc(void* ptr, std::size_t n, std::size_t alignment = Granularity)
    {
        if (!ptr)
            return;

        if (!std::has_single_bit(alignment) || alignment > PageSize) {
            return;
        }

        alignment = std::max(alignment, Granularity);

        std::size_t alignedN = align_up(n, alignment);
        alignedN = align_up(alignedN, Granularity);

        if (alignedN > MaxSmallSize || alignment > MAX_SMALL_ALIGNMENT) {
            std::size_t pageReq = align_up(n, PageSize);
            palloc.dealloc(ptr, pageReq);
            return;
        }

        page_header* page = get_page_header(ptr);
        std::size_t currentTid = troutines.thread_id();

        // Fast path: local thread free
        if (page->owning_thread_id == currentTid) {
            auto& tcache = troutines.get_tcache();
            auto* node = static_cast<free_node*>(ptr);
            node->next = tcache.local_free[page->bucket_idx];
            tcache.local_free[page->bucket_idx] = node;
            page->used_count--;
        }
        // Lock-free path: cross-thread push
        else {
            auto* node = static_cast<free_node*>(ptr);
            free_node* oldHead
                = page->cross_thread_free.load(std::memory_order_relaxed);
            do {
                node->next = oldHead;
            } while (!page->cross_thread_free.compare_exchange_weak(
                oldHead,
                node,
                std::memory_order_release,
                std::memory_order_relaxed
            ));
        }
    }
};

} // namespace glox
