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
#include <cstring>
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
    bool is_large {
        false
    }; // Distinguishes large allocations from small blocks
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

    static constexpr std::size_t align_up(
        std::size_t value,
        std::size_t alignment
    ) noexcept
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

    void collect_and_sweep(
        thread_cache<NUM_BUCKETS>& tcache,
        std::size_t bucket_idx
    )
    {
        page_header** prevPtr = &tcache.pages[bucket_idx];
        page_header* curr = tcache.pages[bucket_idx];

        while (curr != nullptr) {
            if (curr->cross_thread_free.load(std::memory_order_relaxed)
                != nullptr) {
                free_node* stolen = curr->cross_thread_free.exchange(
                    nullptr, std::memory_order_acquire
                );

                std::uint32_t freedCount = 0;
                free_node* tail = stolen;
                while (tail != nullptr) {
                    freedCount++;
                    if (tail->next == nullptr)
                        break;
                    tail = tail->next;
                }

                if (stolen != nullptr) {
                    tail->next = tcache.local_free[bucket_idx];
                    tcache.local_free[bucket_idx] = stolen;
                    curr->used_count -= freedCount;
                }
            }

            page_header* next = curr->next_page;
            if (curr->used_count == 0 && curr != tcache.pages[bucket_idx]) {
                *prevPtr = next;
                palloc.dealloc(curr, PageSize);
            } else {
                prevPtr = &curr->next_page;
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
        collect_and_sweep(tcache, bucket_idx);

        if (tcache.local_free[bucket_idx] != nullptr) {
            free_node* block = tcache.local_free[bucket_idx];
            tcache.local_free[bucket_idx] = block->next;
            page_header* page = get_page_header(block);
            page->used_count++;
            return block;
        }

        auto handle = palloc.alloc(PageSize);
        if (handle.ptr == nullptr) {
            return nullptr;
        }

        auto* page = new (handle.ptr) page_header();
        page->cross_thread_free.store(nullptr, std::memory_order_relaxed);
        page->owning_thread_id = tid;
        page->bucket_idx = bucket_idx;
        page->block_size = get_block_size(bucket_idx);
        page->used_count = 1;
        page->is_large = false;

        page->next_page = tcache.pages[bucket_idx];
        tcache.pages[bucket_idx] = page;

        std::size_t offset = align_up(sizeof(page_header), MAX_SMALL_ALIGNMENT);
        void* firstBlock = static_cast<char*>(handle.ptr) + offset;
        offset += page->block_size;

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

        // Large Allocation Path (now builds a large header)
        if (alignedN > MaxSmallSize || alignment > MAX_SMALL_ALIGNMENT) {
            std::size_t offset = align_up(
                sizeof(page_header), std::max(alignment, alignof(page_header))
            );
            std::size_t pageReq = align_up(n + offset, PageSize);
            auto handle = palloc.alloc(pageReq);
            if (!handle.ptr) {
                return { nullptr, 0 };
            }

            page_header* page = new (handle.ptr) page_header();
            page->is_large = true;
            page->block_size = pageReq;
            page->owning_thread_id = troutines.thread_id();

            return { static_cast<char*>(handle.ptr) + offset,
                pageReq - offset };
        }

        // Small Allocation Path
        std::size_t bucketIdx = get_bucket_index(alignedN);
        auto& tcache = troutines.get_tcache();
        free_node* block = tcache.local_free[bucketIdx];

        if (block != nullptr) {
            tcache.local_free[bucketIdx] = block->next;
            page_header* page = get_page_header(block);
            page->used_count++;
            return { block, get_block_size(bucketIdx) };
        }

        void* ptr
            = allocate_slow_path(tcache, bucketIdx, troutines.thread_id());
        if (ptr == nullptr) {
            return { nullptr, 0 };
        }

        return { ptr, get_block_size(bucketIdx) };
    }

    // Resolves the true payload capacity of an allocated block
    std::size_t usable_size(void* ptr) noexcept
    {
        if (!ptr)
            return 0;
        page_header* page = get_page_header(ptr);

        if (page->is_large) {
            std::size_t offset
                = static_cast<char*>(ptr) - reinterpret_cast<char*>(page);
            return page->block_size - offset;
        }
        return page->block_size;
    }

    // Size-less deallocation
    void dealloc(void* ptr)
    {
        if (!ptr)
            return;

        page_header* page = get_page_header(ptr);
        std::size_t currentTid = troutines.thread_id();

        if (page->is_large) {
            palloc.dealloc(page, page->block_size);
            return;
        }

        if (page->owning_thread_id == currentTid) {
            auto& tcache = troutines.get_tcache();
            auto* node = static_cast<free_node*>(ptr);
            node->next = tcache.local_free[page->bucket_idx];
            tcache.local_free[page->bucket_idx] = node;
            page->used_count--;
        } else {
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

    // Retained for backward API compatibility
    void dealloc(
        void* ptr,
        [[maybe_unused]] std::size_t n,
        [[maybe_unused]] std::size_t alignment = Granularity
    )
    {
        dealloc(ptr);
    }

    void* grow_inplace(void* ptr, std::size_t new_size)
    {
        if (!ptr)
            return nullptr;

        if (new_size <= usable_size(ptr)) {
            return ptr;
        }
        return nullptr;
    }

    void* shrink_inplace(void* ptr, std::size_t new_size)
    {
        if (!ptr)
            return nullptr;
        if (new_size <= usable_size(ptr)) {
            return ptr;
        }
        return nullptr;
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

        std::size_t currentUsable = usable_size(ptr);

        // Fast path: it already fits.
        if (new_size <= currentUsable) {
            return { ptr, currentUsable };
        }

        page_header* page = get_page_header(ptr);

        // If it's a large allocation, ask palloc to grow it
        if (page->is_large) {
            std::size_t offset
                = static_cast<char*>(ptr) - reinterpret_cast<char*>(page);
            std::size_t newPageReq = align_up(new_size + offset, PageSize);

            auto handle = palloc.grow(page, page->block_size, newPageReq);
            if (handle.ptr) {
                page_header* newPage = static_cast<page_header*>(handle.ptr);
                newPage->block_size = newPageReq;
                return { static_cast<char*>(handle.ptr) + offset,
                    newPageReq - offset };
            }
        }

        // Slow path: palloc failed to grow, or it's a small block that needs to
        // become larger
        auto newHandle = alloc(new_size, alignment);
        if (newHandle.ptr) {
            std::memcpy(newHandle.ptr, ptr, currentUsable);
            dealloc(ptr);
        }
        return newHandle;
    }

    alloc_handle<void> shrink(
        void* ptr,
        std::size_t new_size,
        [[maybe_unused]] std::size_t alignment = Granularity
    )
    {
        if (!ptr)
            return { nullptr, 0 };
        if (new_size == 0) {
            dealloc(ptr);
            return { nullptr, 0 };
        }

        std::size_t currentUsable = usable_size(ptr);
        page_header* page = get_page_header(ptr);

        // For large allocations, we can try to release the tail pages via
        // palloc
        if (page->is_large) {
            std::size_t offset
                = static_cast<char*>(ptr) - reinterpret_cast<char*>(page);
            std::size_t newPageReq = align_up(new_size + offset, PageSize);

            if (newPageReq < page->block_size) {
                auto handle = palloc.shrink(page, page->block_size, newPageReq);
                if (handle.ptr) {
                    page = static_cast<page_header*>(handle.ptr);
                    page->block_size = newPageReq;
                    return { static_cast<char*>(handle.ptr) + offset,
                        newPageReq - offset };
                }
            }
        }

        // We don't bother moving small blocks to smaller buckets on shrink, as
        // the overhead of memcpy outweighs the minor space gain. Just treat it
        // as an in-place success.
        return { ptr, currentUsable };
    }
};

} // namespace glox
