#pragma once
#include <glox/assert.hpp>
namespace glox {
// keep for now, remove and replace legacy code once intrusive_list is tested
template <typename T>
struct node : public T
{
    node<T>* prev;
    node<T>* next;

    struct iterator
    {
        node<T>* it;
        auto operator++()
        {
            return it = it->next;
        }
        auto operator--()
        {
            return it = it->prev;
        }
        friend auto operator<=>(iterator, iterator) = default;
        auto& operator*() const
        {
            return *it;
        }
        auto& operator->() const
        {
            return it;
        }
    };
    iterator begin()
    {
        return { this };
    }
    iterator end()
    {
        return { nullptr };
    }

    friend auto operator<=>(const node<T>& l, const node<T>& r)
    {
        return static_cast<T>(l) <=> static_cast<T>(r);
    }

    void remove()
    {
        if (prev != nullptr)
            prev->next = this->next;
        if (next != nullptr)
            next->prev = this->prev;
    }
    void insert(node<T>* left, node<T>* right)
    {
        // gloxAssert(left != nullptr && right != nullptr);
        //  temporary fix
        if (left != nullptr)
            left->next = this;
        if (right != nullptr)
            right->prev = this;
        this->next = right;
        this->prev = left;
    }
};

// legacy
template <typename T>
struct list
{
    node<T>* front;
    node<T>* back;
    struct iterator
    {
        node<T>* it;
        auto operator++()
        {
            return it = it->next;
        }
        auto operator--()
        {
            return it = it->prev;
        }
        friend auto operator<=>(iterator, iterator) = default;
        auto& operator*() const
        {
            return *it;
        }
        auto& operator->() const
        {
            return it;
        }
    };
    iterator begin()
    {
        return { front };
    }
    iterator end()
    {
        return { nullptr };
    }
};
} // namespace glox
