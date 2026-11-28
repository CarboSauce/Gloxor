#pragma once

#ifndef USE_MODULES
#include "glox/alloc.hpp"
#include "glox/assert.hpp"
#include "glox/macros.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>
#endif

namespace glox {
GLOX_BEGIN_EXPORT

// Default RCU / deferred reclamation policy:
// In a single-core / locked setup, immediate deletion is safe.
// On SMP with concurrent lock-free readers, defer `fn()` until an RCU grace
// period elapses.
struct immediate_reclaim_policy
{
    template <typename Fn>
    static void retire(Fn&& fn) noexcept
    {
        FORWARD(fn)();
    }
};

/**
 * @brief Sparse 64-bit radix tree mapping uint64_t keys (e.g. PFNs or page
 * indices) to T.
 *
 * - Integrates directly with glox::allocator<Alloc, node> (defaults to
 * glox::default_allocator<void>).
 * - Dynamic tree height: starts at height 0 (empty) and grows upward only as
 * keys require.
 * - Lock-free reads via find() using acquire/release atomic publication.
 * - Writers (insert / find_or_insert / erase) must be externally serialized
 * (e.g. IrqSpinlock).
 */
template <
    typename T,
    typename Alloc = default_allocator<void>,
    typename Reclaim = immediate_reclaim_policy,
    unsigned BitsPerLevel
    = 6 // 64 slots per node (512B node for 8B T -> 8 cache lines)
>
class radix_tree
{
    static_assert(BitsPerLevel >= 2 && BitsPerLevel <= 8);
    static_assert(std::is_trivially_copyable_v<T>);
    static_assert(
        std::atomic<T>::is_always_lock_free,
        "radix_tree<T> requires T to be always lock-free atomic"
    );

    static constexpr size_t SLOTS_PER_NODE = size_t { 1 } << BitsPerLevel;
    static constexpr uint64_t SLOT_MASK = SLOTS_PER_NODE - 1;
    static constexpr unsigned MAX_LEVELS
        = (64 + BitsPerLevel - 1) / BitsPerLevel;

    struct node
    {
        // Height 0 = leaf node (holds std::atomic<T> values)
        // Height > 0 = internal node (holds std::atomic<node*> child pointers)
        unsigned height;
        unsigned count; // Number of non-null entries (for pruning on erase)

        union
        {
            std::atomic<node*> children[SLOTS_PER_NODE];
            std::atomic<T> values[SLOTS_PER_NODE];
        };

        explicit node(unsigned h) noexcept
            : height(h)
            , count(0)
        {
            if (h == 0) {
                for (size_t i = 0; i < SLOTS_PER_NODE; ++i)
                    std::construct_at(&values[i], T { });
            } else {
                for (size_t i = 0; i < SLOTS_PER_NODE; ++i)
                    std::construct_at(&children[i], nullptr);
            }
        }
    };

    using node_allocator_t = typename Alloc::template rebind<node>::other;
    static_assert(glox::basic_allocator<node_allocator_t, node>);

    struct root_state
    {
        node* ptr = nullptr;
        unsigned height = 0;
    };

    // Pack pointer (low 58 bits) with height in top 6 bits.
    // Sign-extending bit 57 restores canonical x86-64/ARM64 high-half kernel
    // pointers.
    static constexpr unsigned HEIGHT_SHIFT = 58;
    static constexpr uint64_t HEIGHT_MASK = uint64_t { 0x3F } << HEIGHT_SHIFT;

    static uint64_t pack_root(node* p, unsigned h) noexcept
    {
        if (!p)
            return 0;
        auto const raw = reinterpret_cast<uintptr_t>(p);
        return (static_cast<uint64_t>(raw) & ~HEIGHT_MASK)
             | (static_cast<uint64_t>(h) << HEIGHT_SHIFT);
    }

    static root_state unpack_root(uint64_t packed) noexcept
    {
        if (packed == 0)
            return { nullptr, 0 };
        auto const h = static_cast<unsigned>(packed >> HEIGHT_SHIFT);
        auto const raw
            = static_cast<uintptr_t>(static_cast<int64_t>(packed << 6) >> 6);
        return { reinterpret_cast<node*>(raw), h };
    }

    [[no_unique_address]] node_allocator_t alloc_ { };
    std::atomic<uint64_t> root_ { 0 };

public:
    using value_type = T;
    using allocator_type = node_allocator_t;

    constexpr radix_tree() noexcept = default;

    explicit constexpr radix_tree(node_allocator_t const& alloc) noexcept
        : alloc_(alloc)
    {
    }

    radix_tree(radix_tree const&) = delete;
    radix_tree& operator=(radix_tree const&) = delete;

    ~radix_tree() noexcept
    {
        clear();
    }

    bool is_empty() const noexcept
    {
        return root_.load(std::memory_order_acquire) == 0;
    }

    /**
     * @brief Lock-free lookup.
     * Returns pointer to the atomic slot if the path exists, or nullptr if
     * absent.
     */
    std::atomic<T>* find(uint64_t key) noexcept
    {
        return const_cast<std::atomic<T>*>(
            static_cast<radix_tree const*>(this)->find(key)
        );
    }

    std::atomic<T> const* find(uint64_t key) const noexcept
    {
        auto const [currRoot, h]
            = unpack_root(root_.load(std::memory_order_acquire));
        if (!currRoot)
            return nullptr;

        if (key > max_key_for_height(h))
            return nullptr;

        node const* curr = currRoot;
        for (unsigned level = h; level > 0; --level) {
            size_t const idx = index_at(key, level);
            curr = curr->children[idx].load(std::memory_order_acquire);
            if (!curr)
                return nullptr;
        }

        auto const* slot = &curr->values[index_at(key, 0)];
        if (slot->load(std::memory_order_acquire) == T { })
            return nullptr;

        return slot;
    }

    /**
     * @brief Finds an existing slot or allocates missing intermediate/leaf
     * nodes. Caller MUST hold the writer lock.
     */
    struct insert_result
    {
        std::atomic<T>* slot = nullptr;
        bool inserted = false;
    };

    insert_result find_or_insert(uint64_t key) noexcept
    {
        if (!ensure_height_for(key))
            return { nullptr, false };

        auto [curr, h] = unpack_root(root_.load(std::memory_order_relaxed));
        GLOX_ASSERT(curr != nullptr);

        for (unsigned level = h; level > 0; --level) {
            size_t const idx = index_at(key, level);
            node* next = curr->children[idx].load(std::memory_order_relaxed);
            if (!next) {
                next = alloc_node(level - 1);
                if (!next)
                    return { nullptr, false };
                curr->count += 1;
                curr->children[idx].store(next, std::memory_order_release);
            }
            curr = next;
        }

        size_t const leafIdx = index_at(key, 0);
        auto* slot = &curr->values[leafIdx];
        T const existing = slot->load(std::memory_order_relaxed);
        if (existing == T { }) {
            curr->count += 1;
            return { slot, true };
        }
        return { slot, false };
    }

    /**
     * @brief Inserts or overwrites `key` with `val`.
     * Caller MUST hold the writer lock.
     */
    bool insert(uint64_t key, T val) noexcept
    {
        GLOX_ASSERT(val != T { });
        auto [slot, inserted] = find_or_insert(key);
        if (!slot)
            return false;
        slot->store(val, std::memory_order_release);
        return true;
    }

    /**
     * @brief Erases `key` and prunes empty internal/leaf nodes bottom-up.
     * Caller MUST hold the writer lock.
     */
    bool erase(uint64_t key) noexcept
    {
        auto [curr, h] = unpack_root(root_.load(std::memory_order_relaxed));
        if (!curr || key > max_key_for_height(h))
            return false;

        node* path[MAX_LEVELS];
        size_t indices[MAX_LEVELS];

        for (unsigned level = h; level > 0; --level) {
            size_t const idx = index_at(key, level);
            path[level] = curr;
            indices[level] = idx;
            curr = curr->children[idx].load(std::memory_order_relaxed);
            if (!curr)
                return false;
        }

        size_t const leafIdx = index_at(key, 0);
        auto* slot = &curr->values[leafIdx];
        if (slot->load(std::memory_order_relaxed) == T { })
            return false;

        slot->store(T { }, std::memory_order_release);
        curr->count -= 1;

        // Prune empty leaf and ancestor nodes
        node* child = curr;
        for (unsigned level = 1; level <= h; ++level) {
            if (child->count > 0)
                break;
            node* parent = path[level];
            size_t const idx = indices[level];
            parent->children[idx].store(nullptr, std::memory_order_release);
            parent->count -= 1;
            retire_node(child);
            child = parent;
        }

        // If root itself became empty, reset tree to height 0
        if (child->count == 0
            && child
                   == unpack_root(root_.load(std::memory_order_relaxed)).ptr) {
            root_.store(0, std::memory_order_release);
            retire_node(child);
        }

        return true;
    }

    /**
     * @brief Frees all nodes in the tree. Caller must ensure no concurrent
     * readers/writers.
     */
    void clear() noexcept
    {
        auto const [r, h] = unpack_root(root_.load(std::memory_order_relaxed));
        root_.store(0, std::memory_order_relaxed);
        if (r)
            destroy_subtree(r, h);
    }

private:
    static constexpr size_t index_at(uint64_t key, unsigned level) noexcept
    {
        return static_cast<size_t>((key >> (level * BitsPerLevel)) & SLOT_MASK);
    }

    static constexpr uint64_t max_key_for_height(unsigned h) noexcept
    {
        unsigned const totalBits = (h + 1) * BitsPerLevel;
        if (totalBits >= 64)
            return ~uint64_t { 0 };
        return (uint64_t { 1 } << totalBits) - 1;
    }

    bool ensure_height_for(uint64_t key) noexcept
    {
        auto [currRoot, h] = unpack_root(root_.load(std::memory_order_relaxed));

        if (!currRoot) {
            unsigned neededH = 0;
            while (key > max_key_for_height(neededH))
                ++neededH;

            node* fresh = alloc_node(neededH);
            if (!fresh)
                return false;

            root_.store(pack_root(fresh, neededH), std::memory_order_release);
            return true;
        }

        // Grow tree upward one level at a time if key exceeds current root span
        while (key > max_key_for_height(h)) {
            node* newRoot = alloc_node(h + 1);
            if (!newRoot)
                return false;

            newRoot->children[0].store(currRoot, std::memory_order_relaxed);
            newRoot->count = 1;

            currRoot = newRoot;
            ++h;
            root_.store(pack_root(currRoot, h), std::memory_order_release);
        }

        return true;
    }

    node* alloc_node(unsigned height) noexcept
    {
        auto handle = alloc_.alloc(1, alignof(node));
        if (!handle.ptr)
            return nullptr;
        return std::construct_at(handle.ptr, height);
    }

    void free_node(node* n) noexcept
    {
        if (!n)
            return;
        alloc_.dealloc(n, 1, alignof(node));
    }

    void retire_node(node* n) noexcept
    {
        // Capture `this` (or copy `alloc_`) so deferred RCU retirement frees
        // via `alloc_`
        Reclaim::retire([this, n]() noexcept { free_node(n); });
    }

    void destroy_subtree(node* n, unsigned height) noexcept
    {
        if (height > 0) {
            for (size_t i = 0; i < SLOTS_PER_NODE; ++i) {
                node* child = n->children[i].load(std::memory_order_relaxed);
                if (child)
                    destroy_subtree(child, height - 1);
            }
        }
        free_node(n);
    }
};

GLOX_END_EXPORT
} // namespace glox
