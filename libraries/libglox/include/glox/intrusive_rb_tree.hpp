#pragma once
#include "glox/assert.hpp"
#include <cstddef>

namespace glox {
struct rb_tree_node
{
    // maybe we can pack this into parent
    enum class color : bool
    {
        red,
        black
    };

    rb_tree_node* parent;
    rb_tree_node* left;
    rb_tree_node* right;
    color parent_color;
};
template <typename T, rb_tree_node T::* NodePtr = &T::rb_tree_node>
class intrusive_rb_tree
{
    rb_tree_node sentinel;
    size_t tree_size;

    intrusive_rb_tree()
        : sentinel { }
        , tree_size { 0 }
    {
    }

    using color = rb_tree_node::color;

    intrusive_rb_tree(const intrusive_rb_tree&) = delete;
    intrusive_rb_tree& operator=(const intrusive_rb_tree&) = delete;
    intrusive_rb_tree(intrusive_rb_tree&& other)
    {
        sentinel = other.sentinel;
        tree_size = other.tree_size;
        other.sentinel = { &sentinel, &sentinel, &sentinel, color::red };
        other.tree_size = 0;
    }
    intrusive_rb_tree& operator=(intrusive_rb_tree&& other)
    {
        GLOX_ASSERT(other.tree_size);
        sentinel = other.sentinel;
        tree_size = other.tree_size;
        other.sentinel = { &sentinel, &sentinel, &sentinel, color::red };
        other.tree_size = 0;
        return *this;
    }
};
} // namespace glox
