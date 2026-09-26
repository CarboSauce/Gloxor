#pragma once
#include "glox/assert.hpp"
#include "glox/intrusive.hpp"
#include "glox/macros.hpp"
#include <cstddef>
#include <iterator>
#include <type_traits>

namespace glox {
/*
 * @brief List node type, if used for intrusive_list
 * 		  define it as 'list_node'
 */
struct list_node
{
    list_node *next = this, *prev = this;

    friend bool operator==(const list_node&, const list_node&) = default;

    void remove()
    {
        list_node* const n = next;
        list_node* const p = prev;
        p->next = n;
        n->prev = p;
    }
    void insert(list_node* new_current)
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
    static_assert(std::is_same_v<glox::list_node, decltype(T::list_node)>);
    // .prev is last element, .next is first
    list_node sentinel;
    size_t list_size;

    using node_t = list_node;
    // constexpr static auto node_offset = offsetof(T,.*NodePtr);

    template <bool IsConst>
    class iter_base
    {
        using iter = std::conditional_t<IsConst, const list_node*, list_node*>;
        iter it;

    public:
        iter_base(iter p)
            : it(p)
        {
        }
        friend intrusive_list;
        using pointer = std::conditional_t<IsConst, const T*, T*>;
        using reference = std::conditional_t<IsConst, const T&, T&>;
        using iterator_category = std::bidirectional_iterator_tag;

        iter_base() = default;
        iter_base(pointer p)
            : it(&(p->*NodePtr))
        {
        }
        iter_base(const iter_base&) = default;
        iter_base(iter_base&&) = default;
        iter_base& operator=(const iter_base&) = default;
        iter_base& operator=(iter_base&&) = default;

        operator pointer() const
        {
            return intrusive_list::ptr_from_node(it);
        }
        operator iter_base<true>() const
        {
            return iter_base<true>(it);
        }

        iter_base next() const
        {
            return it->next;
        }
        iter_base prev() const
        {
            return it->prev;
        }

        iter_base operator++()
        {
            return it = it->next;
        }
        iter_base operator++(int)
        {
            auto tmp = it;
            it = it->next;
            return tmp;
        }

        iter_base operator--()
        {
            return it = it->prev;
        }
        iter_base operator--(int)
        {
            auto tmp = it;
            it = it->prev;
            return tmp;
        }

        friend bool operator==(iter_base l, iter_base r) = default;

        reference operator*() const
        {
            return *operator->();
        }
        pointer operator->() const
        {
            return static_cast<T*>(*this);
        }
    };

public:
    // struct iterator : public iter_base<false> {using
    // iter_base<false>::iter_base;}; struct const_iterator : public
    // iter_base<true>{using iter_base<true>::iter_base;};
    using iterator = iter_base<false>;
    using const_iterator = iter_base<true>;
    friend iterator;

    intrusive_list()
        : sentinel { }
        , list_size(0)
    {
    }
    intrusive_list(const intrusive_list&) = delete;
    intrusive_list& operator=(const intrusive_list&) = delete;
    intrusive_list(intrusive_list&& other)
    {
        sentinel = other.sentinel;
        list_size = other.list_size;
        other.sentinel = { &sentinel, &sentinel };
        other.list_size = 0;
    }
    intrusive_list& operator=(intrusive_list&& other)
    {
        gloxAssert(other.list_size);
        sentinel = other.sentinel;
        list_size = other.list_size;
        other.sentinel = { &sentinel, &sentinel };
        other.list_size = 0;
        return *this;
    }

    auto size() const
    {
        return list_size;
    }
    iterator begin()
    {
        return sentinel.next;
    }
    // non conformant cuz end()-- is ub
    iterator end()
    {
        return { (node_t*)&sentinel };
    }
    T& back()
    {
        return *ptr_from_node(sentinel.prev);
    }
    T& front()
    {
        return *ptr_from_node(sentinel.next);
    }
    const_iterator begin() const
    {
        return { sentinel.next };
    }
    const_iterator end() const
    {
        return { &sentinel };
    }
    const T& back() const
    {
        return *ptr_from_node(sentinel.prev);
    }
    const T& front() const
    {
        return *ptr_from_node(sentinel.next);
    }
    bool is_empty() const
    {
        return list_size == 0;
    }

    // todo: replace null checks with calls to insert that would check null
    void push_back(T* node)
    {
        impl_insert(&sentinel, &(node->*NodePtr));
    };
    void push_front(T* node)
    {
        impl_insert(sentinel.next, &(node->*NodePtr));
    }
    iterator insert(iterator iter, T* node)
    {
        impl_insert(iter.it, &(node->*NodePtr));
        return node;
    }
    iterator erase(iterator iter)
    {
        iter.it->remove();
        return iter;
    }
    void clear()
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
    void clear(Cb fn)
    {
        for (auto tmp = sentinel.prev; tmp != &sentinel; tmp = tmp->prev) {
            auto saved = tmp->prev;
            fn(ptr_from_node(tmp->remove()));
            tmp = saved;
        }
        sentinel = { &sentinel, &sentinel };
        list_size = 0;
    }

private:
    GLOX_ALWAYS_INLINE
    static const T* ptr_from_node(const list_node* const a)
    {
        return reinterpret_cast<const T*>(
            reinterpret_cast<const char*>(a) - offset_of(NodePtr)
        );
    }
    GLOX_ALWAYS_INLINE
    static T* ptr_from_node(list_node* const a)
    {
        return const_cast<T*>(
            intrusive_list::ptr_from_node(static_cast<const list_node*>(a))
        );
    }
    void impl_insert(list_node* new_next, list_node* new_current)
    {
        new_next->insert(new_current);
        list_size += 1;
    }
};
} // namespace glox
