#pragma once

#ifndef USE_MODULES
#include "alloc.hpp"
#include "detail/moveutils.hpp"
#include "option.hpp"
#include <memory>
#endif

#include "assert.hpp"

namespace glox {
GLOX_BEGIN_EXPORT

template <typename T, size_t BlockSize = 4096, size_t MinElements = 16>
struct deque_block_traits
{
    static constexpr size_t VALUE
        = (sizeof(T) < BlockSize) ? (BlockSize / sizeof(T)) : MinElements;
};

template <
    typename T,
    size_t BlockSize = 4096,
    size_t MinElements = 16,
    typename Allocator = glox::default_allocator<T>
>
class deque
{
public:
    static constexpr size_t BLOCK_SIZE
        = deque_block_traits<T, BlockSize, MinElements>::VALUE;

private:
    T** map = nullptr;
    size_t map_cap = 0;
    size_t start_idx = 0;
    size_t siz = 0;
    [[no_unique_address]] Allocator alloc;

    using map_allocator = typename Allocator::template rebind<T*>::other;
    [[no_unique_address]] map_allocator m_alloc;

    constexpr void allocate_map(size_t new_map_cap)
    {
        T** newMap = glox::alloc_uninitialized<T*>(m_alloc, new_map_cap).ptr;
        for (size_t i = 0; i < new_map_cap; ++i) {
            new (newMap + i) T* { nullptr };
        }
        if (map) {
            size_t startNode = start_idx / BLOCK_SIZE;
            size_t endNode = (start_idx + siz) / BLOCK_SIZE;
            size_t newStartNode = (new_map_cap - (endNode - startNode + 1)) / 2;

            for (size_t i = startNode; i <= endNode; ++i) {
                newMap[newStartNode + (i - startNode)] = map[i];
            }

            start_idx = newStartNode * BLOCK_SIZE + (start_idx % BLOCK_SIZE);
            glox::dealloc(m_alloc, map, map_cap);
        } else {
            start_idx = (new_map_cap / 2) * BLOCK_SIZE;
        }
        map = newMap;
        map_cap = new_map_cap;
    }

    constexpr void ensure_map_capacity(size_t nodes_needed, bool at_front)
    {
        if (map_cap == 0) {
            allocate_map(8); // Initial map size
        }

        size_t startNode = start_idx / BLOCK_SIZE;
        size_t endNode = (start_idx + siz) / BLOCK_SIZE;

        if (at_front && startNode == 0) {
            allocate_map(map_cap * 2);
        } else if (!at_front && endNode + 1 >= map_cap) {
            allocate_map(map_cap * 2);
        }
    }

    constexpr void allocate_node(size_t node_idx)
    {
        if (!map[node_idx]) {
            const auto mem = glox::alloc_uninitialized<T>(alloc, BLOCK_SIZE);
            map[node_idx] = mem.ptr;
        }
    }

    template <bool IsConst>
    class deque_iterator
    {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = std::conditional_t<IsConst, const T*, T*>;
        using reference = std::conditional_t<IsConst, const T&, T&>;
        using map_pointer = std::conditional_t<IsConst, T* const*, T**>;

    private:
        map_pointer map;
        size_t global_idx;

        friend class deque_iterator<!IsConst>;

    public:
        constexpr deque_iterator() = default;
        constexpr deque_iterator(map_pointer m, size_t idx)
            : map(m)
            , global_idx(idx)
        {
        }

        // Implicit conversion from non-const to const iterator
        template <
            bool WasConst = IsConst,
            typename = std::enable_if_t<WasConst>
        >
        constexpr deque_iterator(const deque_iterator<false>& other)
            : map(other.map)
            , global_idx(other.global_idx)
        {
        }

        constexpr reference operator*() const
        {
            return map[global_idx / BLOCK_SIZE][global_idx % BLOCK_SIZE];
        }

        constexpr pointer operator->() const
        {
            return &**this;
        }

        constexpr reference operator[](difference_type n) const
        {
            return *(*this + n);
        }

        constexpr deque_iterator& operator++()
        {
            ++global_idx;
            return *this;
        }

        constexpr deque_iterator operator++(int)
        {
            auto tmp = *this;
            ++global_idx;
            return tmp;
        }

        constexpr deque_iterator& operator--()
        {
            --global_idx;
            return *this;
        }

        constexpr deque_iterator operator--(int)
        {
            auto tmp = *this;
            --global_idx;
            return tmp;
        }

        constexpr deque_iterator& operator+=(difference_type n)
        {
            global_idx += n;
            return *this;
        }

        constexpr deque_iterator& operator-=(difference_type n)
        {
            global_idx -= n;
            return *this;
        }

        constexpr deque_iterator operator+(difference_type n) const
        {
            auto tmp = *this;
            return tmp += n;
        }

        constexpr deque_iterator operator-(difference_type n) const
        {
            auto tmp = *this;
            return tmp -= n;
        }

        constexpr difference_type operator-(const deque_iterator& other) const
        {
            return static_cast<difference_type>(global_idx)
                 - static_cast<difference_type>(other.global_idx);
        }

        constexpr bool operator==(const deque_iterator& other) const
        {
            return global_idx == other.global_idx;
        }
        constexpr bool operator!=(const deque_iterator& other) const
        {
            return global_idx != other.global_idx;
        }
        constexpr bool operator<(const deque_iterator& other) const
        {
            return global_idx < other.global_idx;
        }
        constexpr bool operator>(const deque_iterator& other) const
        {
            return global_idx > other.global_idx;
        }
        constexpr bool operator<=(const deque_iterator& other) const
        {
            return global_idx <= other.global_idx;
        }
        constexpr bool operator>=(const deque_iterator& other) const
        {
            return global_idx >= other.global_idx;
        }

        friend constexpr deque_iterator operator+(
            difference_type n,
            const deque_iterator& it
        )
        {
            return it + n;
        }
    };

public:
    using allocator = Allocator;
    using size_type = std::size_t;
    using reference = T&;
    using const_reference = const T&;
    using iterator = deque_iterator<false>;
    using const_iterator = deque_iterator<true>;

    constexpr deque() = default;

    constexpr deque(const deque& other)
        : alloc(other.alloc)
        , m_alloc(other.m_alloc)
    {
        for (size_t i = 0; i < other.siz; ++i) {
            push_back(other[i]);
        }
    }

    constexpr deque(deque&& other)
        : alloc(RVALUE(other.alloc))
        , m_alloc(RVALUE(other.m_alloc))
    {
        map = other.map;
        map_cap = other.map_cap;
        start_idx = other.start_idx;
        siz = other.siz;

        other.map = nullptr;
        other.map_cap = 0;
        other.start_idx = 0;
        other.siz = 0;
    }

    constexpr deque& operator=(const deque& other)
    {
        if (this != &other) {
            clear();
            for (size_t i = 0; i < other.siz; ++i) {
                push_back(other[i]);
            }
        }
        return *this;
    }

    constexpr deque& operator=(deque&& other)
    {
        using glox::swap;
        swap(map, other.map);
        swap(map_cap, other.map_cap);
        swap(start_idx, other.start_idx);
        swap(siz, other.siz);
        swap(alloc, other.alloc);
        swap(m_alloc, other.m_alloc);
        return *this;
    }

    constexpr ~deque()
    {
        clear();
        if (map) {
            for (size_t i = 0; i < map_cap; ++i) {
                if (map[i]) {
                    glox::dealloc(alloc, map[i], BLOCK_SIZE);
                }
            }
            glox::dealloc(m_alloc, map, map_cap);
        }
    }

    constexpr iterator begin()
    {
        return iterator(map, start_idx);
    }

    constexpr iterator end()
    {
        return iterator(map, start_idx + siz);
    }

    constexpr const_iterator begin() const
    {
        return const_iterator(map, start_idx);
    }

    constexpr const_iterator end() const
    {
        return const_iterator(map, start_idx + siz);
    }

    constexpr const_iterator cbegin() const
    {
        return const_iterator(map, start_idx);
    }

    constexpr const_iterator cend() const
    {
        return const_iterator(map, start_idx + siz);
    }
    constexpr auto size() const
    {
        return siz;
    }
    constexpr auto empty() const
    {
        return siz == 0;
    }

    constexpr auto& front()
    {
        return map[start_idx / BLOCK_SIZE][start_idx % BLOCK_SIZE];
    }
    constexpr const auto& front() const
    {
        return map[start_idx / BLOCK_SIZE][start_idx % BLOCK_SIZE];
    }

    constexpr auto& back()
    {
        size_t lastIdx = start_idx + siz - 1;
        return map[lastIdx / BLOCK_SIZE][lastIdx % BLOCK_SIZE];
    }
    constexpr const auto& back() const
    {
        size_t lastIdx = start_idx + siz - 1;
        return map[lastIdx / BLOCK_SIZE][lastIdx % BLOCK_SIZE];
    }

    constexpr T& operator[](size_t i)
    {
        GLOX_ASSERT(i < siz);
        size_t globalIdx = start_idx + i;
        return map[globalIdx / BLOCK_SIZE][globalIdx % BLOCK_SIZE];
    }

    constexpr const T& operator[](size_t i) const
    {
        GLOX_ASSERT(i < siz);
        size_t globalIdx = start_idx + i;
        return map[globalIdx / BLOCK_SIZE][globalIdx % BLOCK_SIZE];
    }

    template <typename... Args>
    constexpr bool emplace_back(Args&&... args)
    {
        ensure_map_capacity(1, false);
        size_t globalIdx = start_idx + siz;
        size_t node = globalIdx / BLOCK_SIZE;
        allocate_node(node);
        std::construct_at(&map[node][globalIdx % BLOCK_SIZE], FORWARD(args)...);
        ++siz;
        return true;
    }

    template <typename... Args>
    constexpr bool push_back(Args&&... args)
    {
        return emplace_back(FORWARD(args)...);
    }

    template <typename... Args>
    constexpr bool emplace_front(Args&&... args)
    {
        ensure_map_capacity(1, true);
        --start_idx;
        size_t node = start_idx / BLOCK_SIZE;
        allocate_node(node);
        std::construct_at(&map[node][start_idx % BLOCK_SIZE], FORWARD(args)...);
        ++siz;
        return true;
    }

    template <typename... Args>
    constexpr bool push_front(Args&&... args)
    {
        return emplace_front(FORWARD(args)...);
    }

    constexpr void pop_back()
    {
        GLOX_ASSERT(siz > 0);
        size_t lastIdx = start_idx + siz - 1;
        map[lastIdx / BLOCK_SIZE][lastIdx % BLOCK_SIZE].~T();
        --siz;
    }

    constexpr void pop_front()
    {
        GLOX_ASSERT(siz > 0);
        map[start_idx / BLOCK_SIZE][start_idx % BLOCK_SIZE].~T();
        ++start_idx;
        --siz;
    }

    constexpr void clear()
    {
        while (!empty()) {
            pop_back();
        }
        start_idx = (map_cap / 2) * BLOCK_SIZE;
    }

    constexpr bool reserve(size_t new_cap)
    {
        size_t requiredNodes = (new_cap + BLOCK_SIZE - 1) / BLOCK_SIZE;
        if (requiredNodes > map_cap) {
            size_t nextCap = map_cap == 0 ? 8 : map_cap * 2;
            allocate_map(requiredNodes > nextCap ? requiredNodes : nextCap);
        }
        return true;
    }

    constexpr iterator insert(const_iterator pos, T val)
    {
        const size_t offset = pos - cbegin();

        if (offset == 0) {
            emplace_front(RVALUE(val));
            return begin();
        }
        if (offset == siz) {
            emplace_back(RVALUE(val));
            return end() - 1;
        }

        // shift towards the closer end to minimize moves
        if (offset < siz / 2) {
            // shift prefix left
            emplace_front(RVALUE(front()));
            iterator mutPos = begin() + offset;
            glox::move_range(begin() + 2, mutPos + 1, begin() + 1);
            *mutPos = RVALUE(val);
            return mutPos;
        } else {
            // Shift suffix right
            emplace_back(RVALUE(back()));
            iterator mutPos = begin() + offset;
            glox::move_range_backward(mutPos, end() - 2, end() - 1);
            *mutPos = RVALUE(val);
            return mutPos;
        }
    }

    constexpr iterator erase(const_iterator pos)
    {
        const size_t offset = pos - cbegin();

        if (offset == 0) {
            pop_front();
            return begin();
        }
        if (offset == siz - 1) {
            pop_back();
            return end();
        }

        // shift the shorter side to fill the gap
        if (offset < siz / 2) {
            // shift prefix right
            glox::move_range_backward(
                begin(), begin() + offset, begin() + offset + 1
            );
            pop_front();
            return begin() + offset;
        } else {
            // shift suffix left
            glox::move_range(begin() + offset + 1, end(), begin() + offset);
            pop_back();
            return begin() + offset;
        }
    }
};

GLOX_END_EXPORT
} // namespace glox
