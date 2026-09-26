#pragma once
#include <glox/assert.hpp>
#include <iterator>
namespace glox {

// TODO: this is not a intrusive_fwd_list
// should be rewritten to be, and add tests
template <typename T>
struct fwd_list_node
{
    T* next;
};

template <typename T>
class intrusive_fwd_list
{
    T* first_node = nullptr;

public:
    struct iterator
    {
        T* it;

    public:
        using iterator_category = std::forward_iterator_tag;
        iterator() = default;
        iterator(T* p)
            : it(p)
        {
        }
        iterator(const iterator&) = default;
        iterator(iterator&&) = default;
        iterator& operator=(const iterator&) = default;
        iterator& operator=(iterator&&) = default;
        auto operator++()
        {
            return it = it->fwd_list_node.next;
        }
        auto operator++(int)
        {
            auto tmp = it;
            it = it->list_node.next;
            return tmp;
        }
        friend bool operator==(iterator l, iterator r) = default;
        auto& operator*() const
        {
            return *it;
        }
        auto& operator->() const
        {
            return it;
        }
    };
    intrusive_fwd_list() = default;
    intrusive_fwd_list(T* pb)
        : first_node(pb)
    {
    }
    intrusive_fwd_list(const intrusive_fwd_list&) = delete;
    intrusive_fwd_list& operator=(const intrusive_fwd_list&) = delete;
    intrusive_fwd_list(intrusive_fwd_list&& other)
    {
        first_node = other.first_node;
        other.first_node = nullptr;
    }
    intrusive_fwd_list& operator=(intrusive_fwd_list&& other)
    {
        gloxAssert(first_node);
        first_node = other.first_node;
        other.first_node = nullptr;
        return *this;
    }
    iterator begin()
    {
        return { first_node };
    }
    iterator end()
    {
        return { nullptr };
    }
    T& front()
    {
        return *first_node;
    }
    const iterator begin() const
    {
        return { first_node };
    }
    const iterator end() const
    {
        return { nullptr };
    }
    const T& front() const
    {
        return *first_node;
    }
    bool is_empty() const
    {
        return first_node == nullptr;
    }
    void push_front(T* p)
    {
        p->fwd_list_node.next = first_node;
        first_node = p;
    }
    void pop_front()
    {
        if (first_node)
            first_node = first_node->fwd_list_node.next;
    }
    void pop_front_unsafe()
    {
        first_node = first_node->fwd_list_node.next;
    }
    void erase_after(iterator it)
    {
        auto&& oneafter = it->fwd_list_node.next;
        it->fwd_list_node.next = oneafter->fwd_list_node.next;
    }
    void insert_after(iterator it, T* val)
    {
        auto&& tmp = it->fwd_list_node.next;
        val->fwd_list_node.next = tmp->fwd_list_node.next;
        it->fwd_list_node.next = val;
    }
    template <typename Cb>
    void clear(Cb fn)
    {
        while (first_node) {
            auto tmp = first_node;
            first_node = first_node->fwd_list_node.next;
            fn(tmp);
        }
    }
};
} // namespace glox
