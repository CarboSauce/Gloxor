#pragma once

#ifndef USE_MODULES
#include "alloc.hpp"
#include "detail/moveutils.hpp"
#include "glox/hash.hpp"
#include "glox/intrusive_fwd_list.hpp"
#include "glox/vector.hpp"
#include "option.hpp"
#include "util.hpp"
#endif
#include "assert.hpp"

GLOX_EXPORT
namespace glox {

namespace detail {
    template <typename Key, typename Value>
    struct hash_map_node
    {
        glox::pair<const Key, Value> kv;
        glox::fwd_list_node<hash_map_node> fwd_list_node {
            nullptr
        }; // name required by intrusive_fwd_list
    };
} // namespace detail

template <
    typename Key,
    typename Value,
    typename Hash = glox::hash<Key>,
    typename KeyEq = std::equal_to<Key>,
    glox::allocator<detail::hash_map_node<Key, Value>> Alloc
    = glox::default_allocator<detail::hash_map_node<Key, Value>>
>
class hash_map
{
public:
    using hash_type = std::invoke_result_t<Hash, Key>;
    using key_type = Key;
    using value_type = Value;
    using node_type = detail::hash_map_node<Key, Value>;
    using pair_type = glox::pair<const Key, Value>;

private:
    using bucket = intrusive_fwd_list<node_type>;
    vector<bucket> buckets_;
    size_t size_ = 0;
    [[no_unique_address]] Hash hash_;
    [[no_unique_address]] KeyEq eq_;
    [[no_unique_address]] Alloc alloc_;

    static constexpr size_t INITIAL_BUCKETS = 16;

    size_t index_of(hash_type h, size_t bucket_count) const
    {
        return static_cast<size_t>(h) & (bucket_count - 1); // power of two
    }
    size_t index_of_key(const Key& k, size_t bucket_count) const
    {
        return index_of(hash_(k), bucket_count);
    }

    node_type* find_node(const Key& k) const
    {
        if (buckets_.size() == 0)
            return nullptr;
        auto& b = buckets_[index_of_key(k, buckets_.size())];
        for (node_type* n = b.begin().it; n; n = n->fwd_list_node.next)
            if (eq_(n->kv.first, k))
                return n;
        return nullptr;
    }

    template <typename K, typename V>
    node_type* make_node(K&& k, V&& v)
    {
        node_type* n = glox::alloc_uninitialized<node_type>(alloc_, 1).ptr;
        ::new (static_cast<void*>(n))
            node_type { pair_type { static_cast<K&&>(k), static_cast<V&&>(v) },
                glox::fwd_list_node<node_type> { nullptr } };
        return n;
    }

    void destroy_node(node_type* n)
    {
        n->~node_type();
        glox::dealloc(alloc_, n, 1);
    }

    void rehash(size_t new_count)
    {
        gloxAssert((new_count & (new_count - 1)) == 0);
        vector<bucket> nb;
        for (size_t i = 0; i < new_count; ++i)
            nb.push_back(bucket { });

        for (size_t i = 0; i < buckets_.size(); ++i) {

            node_type* n = buckets_[i].begin().it;
            while (n) {
                node_type* next
                    = n->fwd_list_node.next; // push_front overwrites it
                nb[index_of_key(n->kv.first, new_count)].push_front(n);
                n = next;
            }
            buckets_[i] = bucket { };
        }
        buckets_ = static_cast<vector<bucket>&&>(nb);
    }

    void free_all()
    {
        for (size_t i = 0; i < buckets_.size(); ++i)
            buckets_[i].clear([this](node_type* n) { destroy_node(n); });
        size_ = 0;
    }

    void grow_if_needed()
    {
        if (buckets_.size() == 0)
            rehash(INITIAL_BUCKETS);
        else if (size_ + 1 > buckets_.size()) // max load factor 1.0
            rehash(buckets_.size() * 2);
    }

private:
    template <bool IsConst>
    class iter_base
    {
        friend class hash_map;
        friend class iter_base<!IsConst>;

        const vector<bucket>* buckets_ = nullptr;
        size_t idx_ = 0;
        node_type* node_ = nullptr;

        iter_base(const vector<bucket>* b, size_t idx, node_type* n)
            : buckets_(b)
            , idx_(idx)
            , node_(n)
        {
            settle();
        }

        // If the current chain is exhausted, move to the next non-empty bucket.
        void settle()
        {
            while (!node_ && idx_ + 1 < buckets_->size())
                node_ = (*buckets_)[++idx_].begin().it;
        }

    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = pair_type;
        using difference_type = ptrdiff_t;
        using reference
            = std::conditional_t<IsConst, const pair_type&, pair_type&>;
        using pointer
            = std::conditional_t<IsConst, const pair_type*, pair_type*>;

        iter_base() = default;

        // iterator -> const_iterator
        template <bool C = IsConst, typename = std::enable_if_t<C>>
        iter_base(const iter_base<false>& o)
            : buckets_(o.buckets_)
            , idx_(o.idx_)
            , node_(o.node_)
        {
        }

        reference operator*() const
        {
            return node_->kv;
        }
        pointer operator->() const
        {
            return &node_->kv;
        }

        iter_base& operator++()
        {
            node_ = node_->fwd_list_node.next;
            settle();
            return *this;
        }
        iter_base operator++(int)
        {
            iter_base tmp = *this;
            ++*this;
            return tmp;
        }

        friend bool operator==(const iter_base& a, const iter_base& b)
        {
            return a.node_ == b.node_;
        }
    };

public:
    hash_map() = default;
    explicit hash_map(Alloc a)
        : alloc_(static_cast<Alloc&&>(a))
    {
    }
    hash_map(const hash_map&) = delete;
    hash_map& operator=(const hash_map&) = delete;
    hash_map(hash_map&& o)
        : buckets_(static_cast<vector<bucket>&&>(o.buckets_))
        , size_(o.size_)
        , hash_(static_cast<Hash&&>(o.hash_))
        , eq_(static_cast<KeyEq&&>(o.eq_))
        , alloc_(static_cast<Alloc&&>(o.alloc_))
    {
        o.size_ = 0;
    }
    hash_map& operator=(hash_map&& o)
    {
        if (this != &o) {
            free_all();
            buckets_ = static_cast<vector<bucket>&&>(o.buckets_);
            size_ = o.size_;
            hash_ = static_cast<Hash&&>(o.hash_);
            eq_ = static_cast<KeyEq&&>(o.eq_);
            alloc_ = static_cast<Alloc&&>(o.alloc_);
            o.size_ = 0;
        }
        return *this;
    }
    ~hash_map()
    {
        free_all();
    }

    using iterator = iter_base<false>;
    using const_iterator = iter_base<true>;

    iterator begin()
    {
        if (buckets_.size() == 0)
            return end();
        return { &buckets_, 0, buckets_[0].begin().it };
    }
    const_iterator begin() const
    {
        if (buckets_.size() == 0)
            return end();
        return { &buckets_, 0, buckets_[0].begin().it };
    }
    const_iterator cbegin() const
    {
        return begin();
    }

    iterator end()
    {
        return { &buckets_, buckets_.size(), nullptr };
    }
    const_iterator end() const
    {
        return { &buckets_, buckets_.size(), nullptr };
    }
    const_iterator cend() const
    {
        return end();
    }

    size_t size() const
    {
        return size_;
    }
    bool empty() const
    {
        return size_ == 0;
    }
    size_t bucket_count() const
    {
        return buckets_.size();
    }

    glox::option<Value&> find(const Key& k)
    {
        node_type* n = find_node(k);
        if (n)
            return { n->kv.second };
        else
            return { };
    }
    glox::option<const Value&> find(const Key& k) const
    {
        node_type* n = find_node(k);
        if (n)
            return { n->kv.second };
        else
            return { };
    }
    bool contains(const Key& k) const
    {
        return find_node(k) != nullptr;
    }

    template <typename K, typename V>
    bool insert_or_assign(K&& k, V&& v)
    {
        if (node_type* n = find_node(k)) {
            n->kv.second = static_cast<V&&>(v);
            return false;
        }
        grow_if_needed();
        node_type* n = make_node(static_cast<K&&>(k), static_cast<V&&>(v));
        buckets_[index_of_key(n->kv.first, buckets_.size())].push_front(n);
        ++size_;
        return true;
    }

    template <typename K, typename V>
    glox::option<Value&> insert(K&& k, V&& v)
    {
        if (node_type* n = find_node(k))
            return { n->kv.second };

        grow_if_needed();
        node_type* n = make_node(static_cast<K&&>(k), static_cast<V&&>(v));
        buckets_[index_of_key(n->kv.first, buckets_.size())].push_front(n);
        ++size_;
        return { };
    }

    Value& operator[](const Key& k)
    {
        if (node_type* n = find_node(k))
            return n->kv.second;
        grow_if_needed();
        node_type* n = make_node(k, Value { });
        buckets_[index_of_key(n->kv.first, buckets_.size())].push_front(n);
        ++size_;
        return n->kv.second;
    }

    option<Value&> at(const Key& k)
    {
        if (node_type* n = find_node(k))
            return option<Value&> { n->kv.second };
        return option<Value&> { };
    }

    option<const Value&> at(const Key& k) const
    {
        if (node_type* n = find_node(k))
            return option<const Value&> { n->kv.second };
        return option<const Value&> { };
    }

    bool erase(const Key& k)
    {
        if (buckets_.size() == 0)
            return false;
        bucket& b = buckets_[index_of_key(k, buckets_.size())];

        node_type* prev = nullptr;
        for (node_type* n = b.begin().it; n;
            prev = n, n = n->fwd_list_node.next) {
            if (!eq_(n->kv.first, k))
                continue;
            if (prev)
                prev->fwd_list_node.next = n->fwd_list_node.next;
            else
                b.pop_front_unsafe();
            destroy_node(n);
            --size_;
            return true;
        }
        return false;
    }

    iterator find_it(const Key& k)
    {
        node_type* n = find_node(k);
        if (!n)
            return end();
        return { &buckets_, index_of_key(k, buckets_.size()), n };
    }
    const_iterator find_it(const Key& k) const
    {
        node_type* n = find_node(k);
        if (!n)
            return end();
        return { &buckets_, index_of_key(k, buckets_.size()), n };
    }

    iterator erase(iterator it)
    {
        iterator next = it;
        ++next;

        bucket& b = buckets_[it.idx_];
        node_type* prev = nullptr;
        for (node_type* n = b.begin().it; n;
            prev = n, n = n->fwd_list_node.next) {
            if (n != it.node_)
                continue;
            if (prev)
                prev->fwd_list_node.next = n->fwd_list_node.next;
            else
                b.pop_front_unsafe();
            destroy_node(n);
            --size_;
            break;
        }
        return next;
    }

    void clear()
    {
        free_all();
    }
};

} // namespace glox
