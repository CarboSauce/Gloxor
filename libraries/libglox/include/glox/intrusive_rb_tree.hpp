#pragma once

#include "alloc.hpp"
#include "detail/movesem.hpp"
#include "intrusive.hpp"
#include "option.hpp"
#include "util.hpp"

#ifndef USE_MODULES
#include <cstddef>
#include <iterator>
#include <type_traits>
#endif

namespace glox {
GLOX_BEGIN_EXPORT
struct rb_tree_node
{
    // maybe we can pack this into parent
    // but that would require a rewrite
    enum class color : bool
    {
        red,
        black
    };

    rb_tree_node* parent;
    rb_tree_node* left;
    rb_tree_node* right;
    color node_color;
};
namespace detail {
    // layout of the sentinel node owned by each tree:
    // sentinel.parent : the root (null when empty) root->parent == &sentinel
    // sentinel.left   : leftmost node  (== &sentinel when empty) -> begin()
    // sentinel.right  : rightmost node (== &sentinel when empty)
    // sentinel.color  : red Together with sentinel.parent->parent == &sentinel
    //                   this identifies it, since the root is always black
    // end() is the sentinel itself, so ++/-- need no tree pointer
    using node = rb_tree_node;
    using color = node::color;
    using link = node* node::*;

    inline constexpr link link_left = &node::left;
    inline constexpr link link_right = &node::right;

    inline link opposite(link l)
    {
        return l == link_left ? link_right : link_left;
    }

    inline bool is_red(const node* n)
    {
        return n && n->node_color == color::red;
    }

    inline bool is_black(const node* n)
    {
        return !is_red(n);
    }

    inline bool is_sentinel(const node* n)
    {
        return n->node_color == color::red && n->parent
            && n->parent->parent == n;
    }

    inline node* minimum(node* n)
    {
        while (n->left) {
            n = n->left;
        }
        return n;
    }

    inline node* maximum(node* n)
    {
        while (n->right) {
            n = n->right;
        }
        return n;
    }

    // successor, the successor of the last element is the sentinel
    inline node* next(node* x)
    {
        if (x->right) {
            return minimum(x->right);
        }
        node* y = x->parent;
        while (x == y->right) {
            x = y;
            y = y->parent;
        }
        if (x->right != y) {
            x = y;
        }
        return x;
    }

    // predecessor, the predecessor of the sentinel is the last element
    // calling this on begin() is undefined
    inline node* prev(node* x)
    {
        if (is_sentinel(x)) {
            return x->right;
        }
        if (x->left) {
            return maximum(x->left);
        }
        node* y = x->parent;
        while (x == y->left) {
            x = y;
            y = y->parent;
        }
        return y;
    }

    // rotate x down toward side dir, its opp child takes its place.
    // dir equal to link_left is a classic left rotation, link_right a right
    // rotation
    inline void rotate(node* sentinel, node* x, link dir, link opp)
    {
        node* y = x->*opp;
        x->*opp = y->*dir;
        if (y->*dir) {
            (y->*dir)->parent = x;
        }
        y->parent = x->parent;
        if (x == sentinel->parent) {
            sentinel->parent = y;
        } else {
            x->parent->*(x == x->parent->*dir ? dir : opp) = y;
        }
        y->*dir = x;
        x->parent = y;
    }

    // z has already been linked into the tree as a leaf
    inline void insert_fixup(node* sentinel, node* z)
    {
        z->node_color = color::red;
        while (z != sentinel->parent && z->parent->node_color == color::red) {
            node* p = z->parent;
            node* g = p->parent; // real node: p is red, so p is not the root
            link dir = p == g->left ? link_left : link_right;
            link opp = opposite(dir);
            node* uncle = g->*opp;
            if (is_red(uncle)) {
                p->node_color = uncle->node_color = color::black;
                g->node_color = color::red;
                z = g;
            } else {
                if (z == p->*opp) {
                    z = p;
                    rotate(sentinel, z, dir, opp);
                    p = z->parent;
                }
                p->node_color = color::black;
                g->node_color = color::red;
                rotate(sentinel, g, opp, dir);
            }
        }
        sentinel->parent->node_color = color::black;
    }

    // replace the subtree rooted at u with the one rooted at v (v may be null)
    inline void transplant(node* sentinel, node* u, node* v)
    {
        if (u == sentinel->parent) {
            sentinel->parent = v;
        } else if (u == u->parent->left) {
            u->parent->left = v;
        } else {
            u->parent->right = v;
        }
        if (v) {
            v->parent = u->parent;
        }
    }

    // x carries an "extra black", it may be null, so its parent is passed too
    inline void erase_fixup(node* sentinel, node* x, node* parent)
    {
        while (x != sentinel->parent && is_black(x)) {
            link dir = x == parent->left ? link_left : link_right;
            link opp = opposite(dir);
            node* w = parent->*opp; // sibling
            if (is_red(w)) {
                w->node_color = color::black;
                parent->node_color = color::red;
                rotate(sentinel, parent, dir, opp);
                w = parent->*opp;
            }
            if (is_black(w->left) && is_black(w->right)) {
                w->node_color = color::red;
                x = parent;
                parent = x->parent;
            } else {
                if (is_black(w->*opp)) {
                    (w->*dir)->node_color = color::black;
                    w->node_color = color::red;
                    rotate(sentinel, w, opp, dir);
                    w = parent->*opp;
                }
                w->node_color = parent->node_color;
                parent->node_color = color::black;
                (w->*opp)->node_color = color::black;
                rotate(sentinel, parent, dir, opp);
                x = sentinel->parent;
                break;
            }
        }
        if (x) {
            x->node_color = color::black;
        }
    }

    // pure RB unlink, does not maintain sentinel->left / sentinel->right
    inline void erase(node* sentinel, node* z)
    {
        node* y = z;
        color yColor = y->node_color;
        node* x;
        node* xParent;

        if (!z->left) {
            x = z->right;
            xParent = z->parent;
            transplant(sentinel, z, z->right);
        } else if (!z->right) {
            x = z->left;
            xParent = z->parent;
            transplant(sentinel, z, z->left);
        } else {
            y = minimum(z->right);
            yColor = y->node_color;
            x = y->right;
            if (y->parent == z) {
                xParent = y;
            } else {
                xParent = y->parent;
                transplant(sentinel, y, y->right);
                y->right = z->right;
                y->right->parent = y;
            }
            transplant(sentinel, z, y);
            y->left = z->left;
            y->left->parent = y;
            y->node_color = z->node_color;
        }

        if (yColor == color::black) {
            erase_fixup(sentinel, x, xParent);
        }

        z->parent = z->left = z->right = nullptr;
        z->node_color = color::red;
    }

    // returns the black height of the subtree or -1 if an invariant is broken
    // also counts nodes into count
    inline int verify_subtree(const node* n, const node* parent, size_t& count)
    {
        if (!n) {
            return 1;
        }
        ++count;
        if (n->parent != parent) {
            return -1;
        }
        if (is_red(n) && (is_red(n->left) || is_red(n->right))) {
            return -1;
        }
        int l = verify_subtree(n->left, n, count);
        int r = verify_subtree(n->right, n, count);
        if (l < 0 || r < 0 || l != r) {
            return -1;
        }
        return l + (n->node_color == color::black ? 1 : 0);
    }
} // namespace detail

template <typename T, rb_tree_node T::* NodePtr = &T::rb_tree_node>
class intrusive_rb_tree
{
    using node = rb_tree_node;
    using color = node::color;

    node sentinel;
    size_t tree_size;

public:
    using value_type = T;
    using size_type = size_t;

    template <bool IsConst>
    class basic_iterator
    {
    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = T;
        using difference_type = ptrdiff_t;
        using pointer = std::conditional_t<IsConst, const T*, T*>;
        using reference = std::conditional_t<IsConst, const T&, T&>;

        basic_iterator() = default;

        explicit basic_iterator(node* n)
            : current { n }
        {
        }

        // iterator -> const_iterator
        template <
            bool OtherConst,
            typename = std::enable_if_t<IsConst && !OtherConst>
        >
        basic_iterator(const basic_iterator<OtherConst>& other)
            : current { other.current }
        {
        }

        reference operator*() const
        {
            return *to_object(current);
        }

        pointer operator->() const
        {
            return to_object(current);
        }

        basic_iterator& operator++()
        {
            current = detail::next(current);
            return *this;
        }

        basic_iterator operator++(int)
        {
            basic_iterator copy = *this;
            ++*this;
            return copy;
        }

        basic_iterator& operator--()
        {
            current = detail::prev(current);
            return *this;
        }

        basic_iterator operator--(int)
        {
            basic_iterator copy = *this;
            --*this;
            return copy;
        }

        friend bool operator==(const basic_iterator& a, const basic_iterator& b)
        {
            return a.current == b.current;
        }

        friend bool operator!=(const basic_iterator& a, const basic_iterator& b)
        {
            return a.current != b.current;
        }

    private:
        template <bool>
        friend class basic_iterator;
        friend class intrusive_rb_tree;

        node* current = nullptr;
    };

    using iterator = basic_iterator<false>;
    using const_iterator = basic_iterator<true>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    intrusive_rb_tree()
        : sentinel { }
        , tree_size { 0 }
    {
        reset_sentinel();
    }

    intrusive_rb_tree(const intrusive_rb_tree&) = delete;
    intrusive_rb_tree& operator=(const intrusive_rb_tree&) = delete;

    intrusive_rb_tree(intrusive_rb_tree&& other)
        : sentinel { }
        , tree_size { 0 }
    {
        reset_sentinel();
        steal(other);
    }

    intrusive_rb_tree& operator=(intrusive_rb_tree&& other)
    {
        if (this != &other) {
            steal(other);
        }
        return *this;
    }

    // capacity
    bool empty() const
    {
        return tree_size == 0;
    }

    size_t size() const
    {
        return tree_size;
    }

    // iteration
    iterator begin()
    {
        return iterator { sentinel.left };
    }

    const_iterator begin() const
    {
        return const_iterator { sentinel.left };
    }

    const_iterator cbegin() const
    {
        return begin();
    }

    iterator end()
    {
        return iterator { &sentinel };
    }

    const_iterator end() const
    {
        return const_iterator { end_node() };
    }

    const_iterator cend() const
    {
        return end();
    }

    reverse_iterator rbegin()
    {
        return reverse_iterator { end() };
    }

    const_reverse_iterator rbegin() const
    {
        return const_reverse_iterator { end() };
    }

    reverse_iterator rend()
    {
        return reverse_iterator { begin() };
    }

    const_reverse_iterator rend() const
    {
        return const_reverse_iterator { begin() };
    }

    static T* to_object(node* n)
    {
        return container_of(n, NodePtr);
    }

    static const T* to_object(const node* n)
    {
        return container_of(n, NodePtr);
    }

    node* root_node() const
    {
        return sentinel.parent;
    }

    node* end_node() const
    {
        return const_cast<node*>(&sentinel);
    }

    node** root_link()
    {
        return &sentinel.parent;
    }

    iterator insert_at(T& value, node* parent, node** link)
    {
        node* n = &(value.*NodePtr);
        n->parent = parent;
        n->left = n->right = nullptr;
        *link = n;
        if (parent == &sentinel) // first element
        {
            sentinel.left = sentinel.right = n;
        } else if (link == &parent->left && parent == sentinel.left) {
            sentinel.left = n;
        } else if (link == &parent->right && parent == sentinel.right) {
            sentinel.right = n;
        }
        detail::insert_fixup(&sentinel, n);
        ++tree_size;
        return iterator { n };
    }

    // modification
    void erase(T& value)
    {
        GLOX_ASSERT(tree_size);
        node* n = &(value.*NodePtr);
        if (tree_size == 1) {
            detail::erase(&sentinel, n);
            reset_sentinel();
        } else {
            // Compute the new extremes before the structure changes.
            node* newLeft = sentinel.left == n ? detail::next(n) : nullptr;
            node* newRight = sentinel.right == n ? detail::prev(n) : nullptr;
            detail::erase(&sentinel, n);
            if (newLeft) {
                sentinel.left = newLeft;
            }
            if (newRight) {
                sentinel.right = newRight;
            }
        }
        --tree_size;
    }

    iterator erase(const_iterator pos)
    {
        GLOX_ASSERT(pos != end());
        iterator following { pos.current };
        ++following;
        erase(*to_object(pos.current));
        return following;
    }

    // Unlinks every element (without recursion) and calls dispose(T*) on each.
    template <typename Cb>
    void clear(Cb dispose)
    {
        node* n = sentinel.parent;
        while (n) {
            if (n->left) {
                n = n->left;
            } else if (n->right) {
                n = n->right;
            } else {
                node* p = n->parent;
                if (p == &sentinel) {
                    p = nullptr;
                } else {
                    (p->left == n ? p->left : p->right) = nullptr;
                }
                n->parent = n->left = n->right = nullptr;
                n->node_color = color::red;
                dispose(to_object(n));
                n = p;
            }
        }
        reset_sentinel();
        tree_size = 0;
    }

    void clear()
    {
        clear([](T*) { });
    }

    void swap(intrusive_rb_tree& other)
    {
        intrusive_rb_tree tmp { RVALUE(other) };
        other = RVALUE(*this);
        *this = RVALUE(tmp);
    }

    // Checks the structural invariants (for tests / debugging).
    bool verify() const
    {
        if (tree_size == 0) {
            return !sentinel.parent && sentinel.left == &sentinel
                && sentinel.right == &sentinel;
        }
        const node* root = sentinel.parent;
        if (!root || root->node_color != color::black) {
            return false;
        }
        size_t count = 0;
        if (detail::verify_subtree(root, &sentinel, count) < 0
            || count != tree_size) {
            return false;
        }
        return sentinel.left == detail::minimum(sentinel.parent)
            && sentinel.right == detail::maximum(sentinel.parent)
            && root->parent == &sentinel && detail::is_sentinel(&sentinel);
    }

private:
    void reset_sentinel()
    {
        sentinel = { nullptr, &sentinel, &sentinel, color::red };
    }

    // Take over other's elements; this tree's previous contents are forgotten.
    void steal(intrusive_rb_tree& other)
    {
        if (other.tree_size == 0) {
            reset_sentinel();
        } else {
            sentinel = other.sentinel;
            sentinel.parent->parent = &sentinel;
        }
        tree_size = other.tree_size;
        other.reset_sentinel();
        other.tree_size = 0;
    }
};

// Layer 2: still intrusive and non-allocating, but ordered by Compare.
// Compare orders T; make it transparent (like std::less<>) to also look up by
// something other than a T. Unique keys: insert() rejects equivalent elements.
// Built on intrusive_rb_tree, exposing only the operations that cannot break
// the ordering.
template <
    typename T,
    rb_tree_node T::* NodePtr = &T::rb_tree_node,
    typename Compare = std::less<>
>
class ordered_intrusive_rb_tree : private intrusive_rb_tree<T, NodePtr>
{
    using base = intrusive_rb_tree<T, NodePtr>;
    using node = rb_tree_node;

    [[no_unique_address]] Compare comp;

public:
    using value_type = T;
    using size_type = size_t;
    using iterator = typename base::iterator;
    using const_iterator = typename base::const_iterator;
    using reverse_iterator = typename base::reverse_iterator;
    using const_reverse_iterator = typename base::const_reverse_iterator;

    using base::begin;
    using base::cbegin;
    using base::cend;
    using base::clear;
    using base::empty;
    using base::end;
    using base::erase;
    using base::rbegin;
    using base::rend;
    using base::size;

    // Result of a lookup that can be committed with insert_at() without
    // searching again. Invalidated by any modification of the tree.
    struct insert_pos
    {
        node* parent;
        node** link;
        node* existing; // non-null if an equivalent element is already present
    };

    ordered_intrusive_rb_tree()
        : comp { }
    {
    }

    explicit ordered_intrusive_rb_tree(Compare cmp)
        : comp { RVALUE(cmp) }
    {
    }

    ordered_intrusive_rb_tree(const ordered_intrusive_rb_tree&) = delete;
    ordered_intrusive_rb_tree& operator=(const ordered_intrusive_rb_tree&)
        = delete;
    ordered_intrusive_rb_tree(ordered_intrusive_rb_tree&&) = default;
    ordered_intrusive_rb_tree& operator=(ordered_intrusive_rb_tree&&) = default;

    // lookup
    template <typename Key>
    iterator find(const Key& key)
    {
        return iterator { find_node(key) };
    }

    template <typename Key>
    const_iterator find(const Key& key) const
    {
        return const_iterator { find_node(key) };
    }

    template <typename Key>
    iterator lower_bound(const Key& key)
    {
        return iterator { lower_bound_node(key) };
    }

    template <typename Key>
    const_iterator lower_bound(const Key& key) const
    {
        return const_iterator { lower_bound_node(key) };
    }

    template <typename Key>
    iterator upper_bound(const Key& key)
    {
        return iterator { upper_bound_node(key) };
    }

    template <typename Key>
    const_iterator upper_bound(const Key& key) const
    {
        return const_iterator { upper_bound_node(key) };
    }

    // modification
    template <typename Key>
    insert_pos find_insert_pos(const Key& key)
    {
        node* parent = base::end_node();
        node** link = base::root_link();
        while (*link) {
            parent = *link;
            const T* current = base::to_object(parent);
            if (comp(key, *current)) {
                link = &parent->left;
            } else if (comp(*current, key)) {
                link = &parent->right;
            } else {
                return { parent, link, parent };
            }
        }
        return { parent, link, nullptr };
    }

    iterator insert_at(T& value, insert_pos pos)
    {
        return base::insert_at(value, pos.parent, pos.link);
    }

    // inserts value unless an equivalent element already exists
    glox::pair<iterator, bool> insert(T& value)
    {
        insert_pos pos = find_insert_pos(value);
        if (pos.existing) {
            return { iterator { pos.existing }, false };
        }
        return { insert_at(value, pos), true };
    }

    void swap(ordered_intrusive_rb_tree& other)
    {
        base::swap(other);
        std::swap(comp, other.comp);
    }

    const Compare& key_comp() const
    {
        return comp;
    }

    // checks the structural invariants and that elements are strictly ordered
    bool verify() const
    {
        if (!base::verify()) {
            return false;
        }
        const_iterator previous = this->end();
        for (const_iterator it = this->begin(); it != this->end(); ++it) {
            if (previous != this->end() && !comp(*previous, *it)) {
                return false;
            }
            previous = it;
        }
        return true;
    }

private:
    template <typename Key>
    node* lower_bound_node(const Key& key) const
    {
        node* current = base::root_node();
        node* result = base::end_node();
        while (current) {
            if (!comp(*base::to_object(current), key)) {
                result = current;
                current = current->left;
            } else {
                current = current->right;
            }
        }
        return result;
    }

    template <typename Key>
    node* upper_bound_node(const Key& key) const
    {
        node* current = base::root_node();
        node* result = base::end_node();
        while (current) {
            if (comp(key, *base::to_object(current))) {
                result = current;
                current = current->left;
            } else {
                current = current->right;
            }
        }
        return result;
    }

    template <typename Key>
    node* find_node(const Key& key) const
    {
        node* n = lower_bound_node(key);
        return (n != base::end_node() && !comp(key, *base::to_object(n)))
                 ? n
                 : base::end_node();
    }
};

template <typename Key, typename Value>
struct map_entry
{
    rb_tree_node node;
    glox::pair<const Key, Value> kv;

    template <typename K, typename... Args>
    explicit map_entry(K&& key, Args&&... args)
        : node { }
        , kv { FORWARD(key), FORWARD(args)... }
    {
    }
};
enum class insert_result : unsigned char
{
    success,
    key_exists,
    alloc_error
};

template <
    typename Key,
    typename Value,
    typename Compare = std::less<Key>,
    glox::allocator<map_entry<Key, Value>> Allocator
    = default_allocator<map_entry<Key, Value>>
>
class rb_tree_map
{
    using map_entry = map_entry<Key, Value>;

    struct entry_compare
    {
        [[no_unique_address]] Compare comp { };

        bool operator()(const map_entry& a, const map_entry& b) const
        {
            return comp(a.kv.first, b.kv.first);
        }

        bool operator()(const map_entry& a, const Key& b) const
        {
            return comp(a.kv.first, b);
        }

        bool operator()(const Key& a, const map_entry& b) const
        {
            return comp(a, b.kv.first);
        }
    };

    using tree_type
        = ordered_intrusive_rb_tree<map_entry, &map_entry::node, entry_compare>;

    tree_type tree;
    [[no_unique_address]] Allocator alloc;

public:
    using key_type = Key;
    using mapped_type = Value;
    using value_type = glox::pair<const Key, Value>;
    using key_compare = Compare;
    using size_type = size_t;

    template <bool IsConst>
    class basic_iterator
    {
        using base_type = std::conditional_t<
            IsConst,
            typename tree_type::const_iterator,
            typename tree_type::iterator
        >;

    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = glox::pair<const Key, Value>;
        using difference_type = ptrdiff_t;
        using pointer
            = std::conditional_t<IsConst, const value_type*, value_type*>;
        using reference
            = std::conditional_t<IsConst, const value_type&, value_type&>;

        basic_iterator() = default;

        explicit basic_iterator(base_type it)
            : inner { it }
        {
        }

        template <
            bool OtherConst,
            typename = std::enable_if_t<IsConst && !OtherConst>
        >
        basic_iterator(const basic_iterator<OtherConst>& other)
            : inner { other.inner }
        {
        }

        reference operator*() const
        {
            return inner->kv;
        }

        pointer operator->() const
        {
            return &inner->kv;
        }

        basic_iterator& operator++()
        {
            ++inner;
            return *this;
        }

        basic_iterator operator++(int)
        {
            basic_iterator copy = *this;
            ++inner;
            return copy;
        }

        basic_iterator& operator--()
        {
            --inner;
            return *this;
        }

        basic_iterator operator--(int)
        {
            basic_iterator copy = *this;
            --inner;
            return copy;
        }

        friend bool operator==(const basic_iterator& a, const basic_iterator& b)
        {
            return a.inner == b.inner;
        }

        friend bool operator!=(const basic_iterator& a, const basic_iterator& b)
        {
            return a.inner != b.inner;
        }

        base_type base() const
        {
            return inner;
        }

    private:
        template <bool>
        friend class basic_iterator;

        base_type inner;
    };

    using iterator = basic_iterator<false>;
    using const_iterator = basic_iterator<true>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    rb_tree_map() = default;

    explicit rb_tree_map(Compare comp)
        : tree { entry_compare { RVALUE(comp) } }
    {
    }

    rb_tree_map(std::initializer_list<value_type> init)
    {
        for (const value_type& kv : init) {
            insert(kv);
        }
    }

    rb_tree_map(const rb_tree_map& other)
        : tree { other.tree.key_comp() }
    {
        for (const value_type& kv : other) {
            try_emplace(kv.first, kv.second);
        }
    }

    rb_tree_map(rb_tree_map&& other) = default;

    rb_tree_map& operator=(const rb_tree_map& other)
    {
        if (this != &other) {
            rb_tree_map copy { other };
            swap(copy);
        }
        return *this;
    }

    rb_tree_map& operator=(rb_tree_map&& other)
    {
        if (this != &other) {
            clear();
            tree = RVALUE(other.tree);
        }
        return *this;
    }

    ~rb_tree_map()
    {
        clear();
    }

    bool empty() const
    {
        return tree.empty();
    }

    size_t size() const
    {
        return tree.size();
    }

    iterator begin()
    {
        return iterator { tree.begin() };
    }

    const_iterator begin() const
    {
        return const_iterator { tree.begin() };
    }

    const_iterator cbegin() const
    {
        return begin();
    }

    iterator end()
    {
        return iterator { tree.end() };
    }

    const_iterator end() const
    {
        return const_iterator { tree.end() };
    }

    const_iterator cend() const
    {
        return end();
    }

    reverse_iterator rbegin()
    {
        return reverse_iterator { end() };
    }

    const_reverse_iterator rbegin() const
    {
        return const_reverse_iterator { end() };
    }

    reverse_iterator rend()
    {
        return reverse_iterator { begin() };
    }

    const_reverse_iterator rend() const
    {
        return const_reverse_iterator { begin() };
    }

    iterator find(const Key& key)
    {
        return iterator { tree.find(key) };
    }

    const_iterator find(const Key& key) const
    {
        return const_iterator { tree.find(key) };
    }

    bool contains(const Key& key) const
    {
        return find(key) != end();
    }

    size_t count(const Key& key) const
    {
        return contains(key) ? 1 : 0;
    }

    iterator lower_bound(const Key& key)
    {
        return iterator { tree.lower_bound(key) };
    }

    const_iterator lower_bound(const Key& key) const
    {
        return const_iterator { tree.lower_bound(key) };
    }

    iterator upper_bound(const Key& key)
    {
        return iterator { tree.upper_bound(key) };
    }

    const_iterator upper_bound(const Key& key) const
    {
        return const_iterator { tree.upper_bound(key) };
    }

    glox::option<Value&> at(const Key& key)
    {
        iterator it = find(key);
        if (it == end()) {
            return { };
        }
        return it->second;
    }

    glox::option<const Value&> at(const Key& key) const
    {
        const_iterator it = find(key);
        if (it == end()) {
            return { };
        }
        return it->second;
    }

    // modification
    // Constructs the mapped value in place only if the key is absent.
    template <typename... Args>
    glox::pair<iterator, insert_result>
    try_emplace(const Key& key, Args&&... args)
    {
        return try_emplace_impl(key, FORWARD(args)...);
    }

    template <typename... Args>
    glox::pair<iterator, insert_result> try_emplace(Key&& key, Args&&... args)
    {
        return try_emplace_impl(RVALUE(key), FORWARD(args)...);
    }

    template <typename... Args>
    glox::pair<iterator, insert_result> insert(const Key& key, Args&&... args)
    {
        return try_emplace_impl(key, FORWARD(args)...);
    }

    template <typename... Args>
    glox::pair<iterator, insert_result> insert(Key&& key, Args&&... args)
    {
        return try_emplace_impl(RVALUE(key), FORWARD(args)...);
    }

    glox::pair<iterator, insert_result> insert(const value_type& kv)
    {
        return try_emplace(kv.first, kv.second);
    }

    glox::pair<iterator, insert_result> insert(value_type&& kv)
    {
        return try_emplace(kv.first, RVALUE(kv.second));
    }

    template <typename M>
    glox::pair<iterator, insert_result>
    insert_or_assign(const Key& key, M&& mapped)
    {
        auto result = try_emplace(key, FORWARD(mapped));
        if (result.second == insert_result::key_exists) {
            result.first->second = FORWARD(mapped);
        }
        return result;
    }

    iterator erase(const_iterator pos)
    {
        map_entry* e = const_cast<map_entry*>(&*pos.base());
        iterator following { tree.erase(pos.base()) };
        delete e;
        return following;
    }

    size_t erase(const Key& key)
    {
        iterator it = find(key);
        if (it == end()) {
            return 0;
        }
        erase(const_iterator { it });
        return 1;
    }

    void clear()
    {
        tree.clear([](map_entry* e) { delete e; });
    }

    void swap(rb_tree_map& other)
    {
        tree.swap(other.tree);
    }

    key_compare key_comp() const
    {
        return tree.key_comp().comp;
    }

    bool verify() const
    {
        return tree.verify();
    }

private:
    template <typename K, typename... Args>
    glox::pair<iterator, insert_result>
    try_emplace_impl(K&& key, Args&&... args)
    {
        auto pos = tree.find_insert_pos(static_cast<const Key&>(key));
        if (pos.existing) {
            return {
                iterator { typename tree_type::iterator { pos.existing } },
                insert_result::key_exists,
            };
        }
        auto mem = glox::alloc_uninitialized<map_entry>(alloc, 1);
        if (mem.ptr == nullptr) {
            return { end(), insert_result::alloc_error };
        }
        map_entry* e
            = new (mem.ptr) map_entry { FORWARD(key), FORWARD(args)... };
        return {
            iterator { tree.insert_at(*e, pos) },
            insert_result::success,
        };
    }
};
GLOX_END_EXPORT
} // namespace glox
