#pragma once

#ifndef USE_MODULES
#include "glox/mutex.hpp"
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#endif

namespace glox {
GLOX_BEGIN_EXPORT

struct memory_region
{
    std::uintptr_t phys_addr;
    std::size_t size;
};

template <typename Desc>
concept pfn_descriptor = requires(Desc d, const Desc cd, std::size_t order) {
    { Desc::make_reserved() } -> std::same_as<Desc>;
    { Desc::make_buddy_free(order) } -> std::same_as<Desc>;
    { Desc::make_allocated(order) } -> std::same_as<Desc>;
    { cd.is_buddy_free() } -> std::convertible_to<bool>;
    { cd.buddy_order() } -> std::convertible_to<std::size_t>;
};

template <typename Accessor>
concept pfndb_accessor
    = requires(Accessor& acc, const Accessor& cacc, std::uintptr_t pfn) {
          requires pfn_descriptor<std::remove_cvref_t<decltype(acc[pfn])>>;
          {
              acc[pfn]
          } -> std::same_as<std::remove_cvref_t<decltype(acc[pfn])>&>;
          {
              cacc[pfn]
          } -> std::same_as<const std::remove_cvref_t<decltype(acc[pfn])>&>;
          { cacc.is_valid(pfn) } -> std::convertible_to<bool>;
          { cacc.pfn_to_virt(pfn) } -> std::convertible_to<void*>;
          { Accessor::TAG_BUDDY } -> std::convertible_to<uintptr_t>;
          { Accessor::TAG_META } -> std::convertible_to<uintptr_t>;
          { Accessor::TAG_SLAB } -> std::convertible_to<uintptr_t>;
          { Accessor::TAG_LARGE } -> std::convertible_to<uintptr_t>;
          { Accessor::TAG_LARGE_TAIL } -> std::convertible_to<uintptr_t>;
      };

template <
    pfndb_accessor Accessor,
    glox::mutex_type Mutex = glox::spinlock,
    std::size_t PageShift = 12,
    std::size_t MaxOrder = 11 // orders 0..10 (up to 4 mb contiguous blocks)
>
class buddy_allocator
{
    static_assert(PageShift >= 12, "Page size must be at least 4 KiB");
    static_assert(MaxOrder >= 1 && MaxOrder <= 20, "Invalid MaxOrder");

public:
    using accessor_type = Accessor;
    using descriptor_type
        = std::remove_reference_t<decltype(std::declval<Accessor&>()[0])>;

    static constexpr std::size_t PAGE_SIZE = std::size_t { 1 } << PageShift;
    static constexpr auto PAGE_SHIFT = PageShift;
    static constexpr std::size_t MAX_ORDER = MaxOrder;

private:
    struct free_node
    {
        std::uintptr_t prev_pfn; // 0 = list head
        std::uintptr_t next_pfn; // 0 = list tail
    };

    [[no_unique_address]] mutable Mutex lock_ { };
    [[no_unique_address]] Accessor pfndb_ { };
    std::size_t free_pages_ = 0;
    std::uintptr_t free_lists_[MaxOrder] { };

    [[nodiscard]] free_node* node_at(std::uintptr_t pfn)
    {
        return static_cast<free_node*>(pfndb_.pfn_to_virt(pfn));
    }

    void list_push_front(std::size_t order, std::uintptr_t pfn)
    {
        free_node* node = node_at(pfn);
        const std::uintptr_t oldHead = free_lists_[order];

        node->prev_pfn = 0;
        node->next_pfn = oldHead;

        if (oldHead != 0) {
            node_at(oldHead)->prev_pfn = pfn;
        }

        free_lists_[order] = pfn;
        pfndb_[pfn] = descriptor_type::make_buddy_free(order);
    }

    void list_remove(std::size_t order, std::uintptr_t pfn)
    {
        free_node* node = node_at(pfn);
        const std::uintptr_t prev = node->prev_pfn;
        const std::uintptr_t next = node->next_pfn;

        if (prev != 0) {
            node_at(prev)->next_pfn = next;
        } else {
            free_lists_[order] = next;
        }

        if (next != 0) {
            node_at(next)->prev_pfn = prev;
        }

        pfndb_[pfn] = descriptor_type::make_reserved();
    }

    [[nodiscard]] std::uintptr_t list_pop_front(std::size_t order)
    {
        const std::uintptr_t head = free_lists_[order];
        if (head != 0) {
            list_remove(order, head);
        }
        return head;
    }

    void release_block(std::uintptr_t pfn, std::size_t order)
    {
        while (order + 1 < MaxOrder) {
            const std::uintptr_t buddyPfn
                = pfn ^ (std::uintptr_t { 1 } << order);

            if (buddyPfn == 0 || !pfndb_.is_valid(buddyPfn)) {
                break;
            }

            const descriptor_type buddy = pfndb_[buddyPfn];
            if (!buddy.is_buddy_free() || buddy.buddy_order() != order) {
                break;
            }

            list_remove(order, buddyPfn);
            if (buddyPfn < pfn) {
                pfn = buddyPfn;
            }
            ++order;
        }

        list_push_front(order, pfn);
    }

public:
    buddy_allocator() = default;

    explicit buddy_allocator(Accessor accessor)
        : pfndb_(std::move(accessor))
    {
    }

    buddy_allocator(const buddy_allocator&) = delete;
    buddy_allocator& operator=(const buddy_allocator&) = delete;

    void set_accessor(Accessor accessor)
    {
        scoped_lock guard(lock_);
        pfndb_ = std::move(accessor);
    }

    [[nodiscard]] Accessor& accessor()
    {
        return pfndb_;
    }
    [[nodiscard]] const Accessor& accessor() const
    {
        return pfndb_;
    }

    // Append an arbitrary {phys_addr, size} chunk of usable physical memory.
    void add_region(memory_region region)
    {
        if (region.size == 0) {
            return;
        }

        std::uintptr_t start
            = (region.phys_addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        const std::uintptr_t end
            = (region.phys_addr + region.size) & ~(PAGE_SIZE - 1);

        if (start == 0) {
            start = PAGE_SIZE;
        }
        if (end <= start) {
            return;
        }

        std::uintptr_t curPfn = start >> PageShift;
        const std::uintptr_t endPfn = end >> PageShift;

        scoped_lock guard(lock_);

        while (curPfn < endPfn) {
            const std::uintptr_t remaining = endPfn - curPfn;

            std::size_t alignOrder
                = static_cast<std::size_t>(std::countr_zero(curPfn));
            if (alignOrder >= MaxOrder) {
                alignOrder = MaxOrder - 1;
            }

            std::size_t sizeOrder
                = static_cast<std::size_t>(std::bit_width(remaining) - 1);
            if (sizeOrder >= MaxOrder) {
                sizeOrder = MaxOrder - 1;
            }

            const std::size_t order
                = alignOrder < sizeOrder ? alignOrder : sizeOrder;
            const std::uintptr_t blockPfns = std::uintptr_t { 1 } << order;

            if (!pfndb_.is_valid(curPfn)) {
                break;
            }

            release_block(curPfn, order);
            free_pages_ += blockPfns;
            curPfn += blockPfns;
        }
    }

    void add_region(std::uintptr_t phys_addr, std::size_t size)
    {
        add_region(memory_region { phys_addr, size });
    }

    [[nodiscard]] std::uintptr_t alloc_order(std::size_t order)
    {
        if (order >= MaxOrder) {
            return 0;
        }

        scoped_lock guard(lock_);

        std::size_t currentOrder = order;
        while (currentOrder < MaxOrder && free_lists_[currentOrder] == 0) {
            ++currentOrder;
        }

        if (currentOrder == MaxOrder) {
            return 0;
        }

        const std::uintptr_t pfn = list_pop_front(currentOrder);

        while (currentOrder > order) {
            --currentOrder;
            const std::uintptr_t buddyPfn
                = pfn + (std::uintptr_t { 1 } << currentOrder);
            list_push_front(currentOrder, buddyPfn);
        }

        pfndb_[pfn] = descriptor_type::make_allocated(order);
        free_pages_ -= (std::size_t { 1 } << order);
        return pfn << PageShift;
    }

    [[nodiscard]] std::uintptr_t alloc_bytes(std::size_t bytes)
    {
        if (bytes == 0) {
            bytes = 1;
        }
        const std::size_t pages = (bytes + PAGE_SIZE - 1) >> PageShift;
        const std::size_t order
            = pages <= 1 ? 0
                         : static_cast<std::size_t>(std::bit_width(pages - 1));
        return alloc_order(order);
    }

    void free_order(std::uintptr_t phys_addr, std::size_t order)
    {
        if (phys_addr == 0 || order >= MaxOrder
            || (phys_addr & (PAGE_SIZE - 1)) != 0) {
            return;
        }

        const std::uintptr_t pfn = phys_addr >> PageShift;
        if (!pfndb_.is_valid(pfn)) {
            return;
        }

        scoped_lock guard(lock_);
        free_pages_ += (std::size_t { 1 } << order);
        release_block(pfn, order);
    }

    void free_bytes(std::uintptr_t phys_addr, std::size_t bytes)
    {
        if (bytes == 0) {
            bytes = 1;
        }
        const std::size_t pages = (bytes + PAGE_SIZE - 1) >> PageShift;
        const std::size_t order
            = pages <= 1 ? 0
                         : static_cast<std::size_t>(std::bit_width(pages - 1));
        free_order(phys_addr, order);
    }

    // orderless free: reads the order recorded in the descriptor when allocated
    void free(std::uintptr_t phys_addr)
    {
        if (phys_addr == 0 || (phys_addr & (PAGE_SIZE - 1)) != 0) {
            return;
        }

        const std::uintptr_t pfn = phys_addr >> PageShift;
        if (!pfndb_.is_valid(pfn)) {
            return;
        }

        scoped_lock guard(lock_);
        const descriptor_type desc = pfndb_[pfn];
        if (desc.is_buddy_free()) {
            return;
        }

        const std::size_t order = desc.buddy_order();
        if (order >= MaxOrder) {
            return;
        }

        free_pages_ += (std::size_t { 1 } << order);
        release_block(pfn, order);
    }

    [[nodiscard]] std::size_t free_pages() const
    {
        scoped_lock guard(lock_);
        return free_pages_;
    }

    [[nodiscard]] std::size_t free_bytes() const
    {
        return free_pages() << PageShift;
    }
};

GLOX_END_EXPORT
} // namespace glox
