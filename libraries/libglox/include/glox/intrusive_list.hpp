#pragma once
#include "glox/assert.hpp"
#include "glox/intrusive.hpp"
#include "glox/macros.hpp"

#ifndef USE_MODULES
#include <cstddef>
#include <iterator>
#include <type_traits>
#endif

namespace glox {
GLOX_BEGIN_EXPORT

/*
 * @brief List node type, if used for intrusive_list
 *        define it as 'list_node'
 */
struct list_node
{
    list_node *next = this, *prev = this;

    friend bool operator==(const list_node&, const list_node&) = default;

    list_node* remove() noexcept
    {
        list_node* const n = next;
        list_node* const p = prev;
        p->next = n;
        n->prev = p;
        next = this;
        prev = this;
        return this;
    }

    void insert(list_node* new_current) noexcept
    {
        new_current->next = this;
        new_current->prev = this->prev;
        this->prev->next = new_current;
        this->prev = new_current;
    }
};

template <typename T, list_node T::* NodePtr = &T::list_node>
class intrusive_list
{
    // .prev is last element, .next is first
    list_node sentinel;
    size_t list_size;

    using node_t = list_node;

    template <bool IsConst>
    class iter_base
    {
        using iter = std::conditional_t<IsConst, const list_node*, list_node*>;
        iter it = nullptr;

    public:
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = std::conditional_t<IsConst, const T*, T*>;
        using reference = std::conditional_t<IsConst, const T&, T&>;
        using iterator_category = std::bidirectional_iterator_tag;
        using iterator_concept = std::bidirectional_iterator_tag;

        friend intrusive_list;
        friend iter_base<!IsConst>;

        constexpr iter_base() noexcept = default;
        constexpr iter_base(iter p) noexcept
            : it(p)
        {
        }
        constexpr iter_base(pointer p) noexcept
            : it(&(p->*NodePtr))
        {
        }
        constexpr iter_base(const iter_base&) noexcept = default;
        constexpr iter_base(iter_base&&) noexcept = default;
        constexpr iter_base& operator=(const iter_base&) noexcept = default;
        constexpr iter_base& operator=(iter_base&&) noexcept = default;

        operator pointer() const noexcept
        {
            return intrusive_list::ptr_from_node(it);
        }

        template <bool WasConst = IsConst>
            requires(!WasConst)
        operator iter_base<true>() const noexcept
        {
            return iter_base<true>(it);
        }

        iter_base next() const noexcept
        {
            return it->next;
        }

        iter_base prev() const noexcept
        {
            return it->prev;
        }

        iter_base& operator++() noexcept
        {
            it = it->next;
            return *this;
        }

        iter_base operator++(int) noexcept
        {
            auto tmp = *this;
            it = it->next;
            return tmp;
        }

        iter_base& operator--() noexcept
        {
            it = it->prev;
            return *this;
        }

        iter_base operator--(int) noexcept
        {
            auto tmp = *this;
            it = it->prev;
            return tmp;
        }

        friend bool operator==(iter_base l, iter_base r) noexcept = default;

        reference operator*() const noexcept
        {
            return *operator->();
        }

        pointer operator->() const noexcept
        {
            return intrusive_list::ptr_from_node(it);
        }
    };

public:
    using iterator = iter_base<false>;
    using const_iterator = iter_base<true>;
    friend iterator;
    friend const_iterator;

    constexpr intrusive_list() noexcept
        : sentinel { }
        , list_size(0)
    {
    }

    intrusive_list(const intrusive_list&) = delete;
    intrusive_list& operator=(const intrusive_list&) = delete;

    intrusive_list(intrusive_list&& other) noexcept
        : sentinel { }
        , list_size(0)
    {
        steal_from(other);
    }

    intrusive_list& operator=(intrusive_list&& other) noexcept
    {
        if (this != &other) {
            clear();
            steal_from(other);
        }
        return *this;
    }

    auto size() const noexcept
    {
        return list_size;
    }

    iterator begin() noexcept
    {
        return sentinel.next;
    }

    iterator end() noexcept
    {
        return { &sentinel };
    }

    T& back() noexcept
    {
        GLOX_ASSERT(list_size > 0);
        return *ptr_from_node(sentinel.prev);
    }

    T& front() noexcept
    {
        GLOX_ASSERT(list_size > 0);
        return *ptr_from_node(sentinel.next);
    }

    const_iterator begin() const noexcept
    {
        return { sentinel.next };
    }

    const_iterator end() const noexcept
    {
        return { &sentinel };
    }

    const T& back() const noexcept
    {
        GLOX_ASSERT(list_size > 0);
        return *ptr_from_node(sentinel.prev);
    }

    const T& front() const noexcept
    {
        GLOX_ASSERT(list_size > 0);
        return *ptr_from_node(sentinel.next);
    }

    bool is_empty() const noexcept
    {
        return list_size == 0;
    }

    void push_back(T* node) noexcept
    {
        GLOX_ASSERT(node != nullptr);
        impl_insert(&sentinel, &(node->*NodePtr));
    }

    void push_front(T* node) noexcept
    {
        GLOX_ASSERT(node != nullptr);
        impl_insert(sentinel.next, &(node->*NodePtr));
    }

    iterator insert(iterator iter, T* node) noexcept
    {
        GLOX_ASSERT(node != nullptr);
        impl_insert(iter.it, &(node->*NodePtr));
        return node;
    }

    iterator erase(iterator iter) noexcept
    {
        GLOX_ASSERT(list_size > 0 && iter.it != &sentinel);
        auto nextIt = iter.next();
        iter.it->remove();
        --list_size;
        return nextIt;
    }

    void clear() noexcept
    {
        for (auto tmp = sentinel.prev; tmp != &sentinel;) {
            auto saved = tmp->prev;
            tmp->remove();
            tmp = saved;
        }
        sentinel = { &sentinel, &sentinel };
        list_size = 0;
    }

    template <typename Cb>
    void clear(Cb&& fn)
    {
        for (auto tmp = sentinel.prev; tmp != &sentinel;) {
            auto saved = tmp->prev;
            fn(ptr_from_node(tmp->remove()));
            tmp = saved;
        }
        sentinel = { &sentinel, &sentinel };
        list_size = 0;
    }

private:
    GLOX_ALWAYS_INLINE
    static const T* ptr_from_node(const list_node* const a) noexcept
    {
        return container_of(a, NodePtr);
    }

    GLOX_ALWAYS_INLINE
    static T* ptr_from_node(list_node* const a) noexcept
    {
        return container_of(a, NodePtr);
    }

    void impl_insert(list_node* new_next, list_node* new_current) noexcept
    {
        new_next->insert(new_current);
        list_size += 1;
    }

    void steal_from(intrusive_list& other) noexcept
    {
        if (other.list_size == 0) {
            sentinel = { &sentinel, &sentinel };
            list_size = 0;
            return;
        }
        sentinel.next = other.sentinel.next;
        sentinel.prev = other.sentinel.prev;
        sentinel.next->prev = &sentinel;
        sentinel.prev->next = &sentinel;
        list_size = other.list_size;

        other.sentinel = { &other.sentinel, &other.sentinel };
        other.list_size = 0;
    }
};

GLOX_END_EXPORT
} // namespace glox
