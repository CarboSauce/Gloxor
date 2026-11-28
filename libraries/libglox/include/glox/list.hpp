#pragma once

#ifndef USE_MODULES
#include "glox/alloc.hpp"
#include "glox/assert.hpp"
#include "glox/detail/moveutils.hpp"
#include "glox/intrusive_list.hpp"
#include "glox/macros.hpp"
#include <cstddef>
#include <iterator>
#include <memory>
#include <type_traits>
#endif

namespace glox {
GLOX_BEGIN_EXPORT

template <typename T, glox::allocator<T> Allocator = glox::default_allocator<T>>
class list
{
    struct node_t
    {
        list_node hook { };
        T value;

        template <typename... Args>
        explicit node_t(Args&&... args)
            : value(FORWARD(args)...)
        {
        }
    };

    using backend_t = intrusive_list<node_t, &node_t::hook>;
    using node_allocator = typename Allocator::template rebind<node_t>::other;

    backend_t backend_ { };
    [[no_unique_address]] Allocator alloc_ { };
    [[no_unique_address]] node_allocator node_alloc_ { };

    template <typename SrcAlloc>
    static constexpr node_allocator rebind_node_alloc(SrcAlloc&& a) noexcept
    {
        if constexpr (std::constructible_from<node_allocator, SrcAlloc>) {
            return node_allocator(FORWARD(a));
        } else {
            return node_allocator { };
        }
    }

    template <typename... Args>
    node_t* create_node(Args&&... args)
    {
        node_t* mem = glox::alloc_uninitialized<node_t>(node_alloc_, 1).ptr;
        GLOX_ASSERT(mem != nullptr);
        new (mem) node_t(FORWARD(args)...);
        return mem;
    }

    void destroy_node(node_t* n) noexcept
    {
        if (n != nullptr) {
            n->~node_t();
            glox::dealloc(node_alloc_, n, 1);
        }
    }

    template <bool IsConst>
    class iter_base
    {
        using inner_t = std::conditional_t<
            IsConst,
            typename backend_t::const_iterator,
            typename backend_t::iterator
        >;

        inner_t it_ { };

        friend class list;
        friend class iter_base<!IsConst>;

    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using iterator_concept = std::bidirectional_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = std::conditional_t<IsConst, T const*, T*>;
        using reference = std::conditional_t<IsConst, T const&, T&>;

        constexpr iter_base() noexcept = default;
        constexpr explicit iter_base(inner_t it) noexcept
            : it_(it)
        {
        }

        // Implicit conversion from iterator to const_iterator
        template <bool WasConst = IsConst>
            requires WasConst
        constexpr iter_base(iter_base<false> const& other) noexcept
            : it_(other.it_)
        {
        }

        constexpr reference operator*() const noexcept
        {
            return (*it_).value;
        }

        constexpr pointer operator->() const noexcept
        {
            return std::addressof((*it_).value);
        }

        constexpr iter_base& operator++() noexcept
        {
            ++it_;
            return *this;
        }

        constexpr iter_base operator++(int) noexcept
        {
            auto tmp = *this;
            ++it_;
            return tmp;
        }

        constexpr iter_base& operator--() noexcept
        {
            --it_;
            return *this;
        }

        constexpr iter_base operator--(int) noexcept
        {
            auto tmp = *this;
            --it_;
            return tmp;
        }

        friend constexpr bool operator==(iter_base a, iter_base b) noexcept
            = default;
    };

public:
    using value_type = T;
    using allocator_type = Allocator;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = T&;
    using const_reference = T const&;
    using pointer = T*;
    using const_pointer = T const*;
    using iterator = iter_base<false>;
    using const_iterator = iter_base<true>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    constexpr list() noexcept = default;

    explicit list(Allocator const& alloc) noexcept
        : alloc_(alloc)
        , node_alloc_(rebind_node_alloc(alloc))
    {
    }

    explicit list(size_type count, Allocator const& alloc = Allocator { })
        : list(alloc)
    {
        for (size_type i = 0; i < count; ++i) {
            emplace_back();
        }
    }

    list(
        size_type count,
        T const& value,
        Allocator const& alloc = Allocator { }
    )
        : list(alloc)
    {
        for (size_type i = 0; i < count; ++i) {
            push_back(value);
        }
    }

    template <std::input_iterator InputIt>
    list(InputIt first, InputIt last, Allocator const& alloc = Allocator { })
        : list(alloc)
    {
        for (; first != last; ++first) {
            emplace_back(*first);
        }
    }

    list(list const& other)
        : alloc_(other.alloc_)
        , node_alloc_(other.node_alloc_)
    {
        for (auto const& item : other) {
            push_back(item);
        }
    }

    list(list&& other) noexcept
        : alloc_(RVALUE(other.alloc_))
        , node_alloc_(RVALUE(other.node_alloc_))
    {
        steal_from(other);
    }

    list& operator=(list const& other)
    {
        if (this != &other) {
            clear();
            for (auto const& item : other) {
                push_back(item);
            }
        }
        return *this;
    }

    list& operator=(list&& other) noexcept
    {
        if (this != &other) {
            clear();
            alloc_ = RVALUE(other.alloc_);
            node_alloc_ = RVALUE(other.node_alloc_);
            steal_from(other);
        }
        return *this;
    }

    ~list()
    {
        clear();
    }

    template <std::input_iterator InputIt>
    void assign(InputIt first, InputIt last)
    {
        auto cur = begin();
        auto end_it = end();
        for (; cur != end_it && first != last; ++cur, ++first) {
            *cur = *first;
        }
        if (first == last) {
            erase(cur, end_it);
        } else {
            for (; first != last; ++first) {
                emplace_back(*first);
            }
        }
    }

    void assign(size_type count, T const& value)
    {
        auto cur = begin();
        auto end_it = end();
        for (; cur != end_it && count > 0; ++cur, --count) {
            *cur = value;
        }
        if (count == 0) {
            erase(cur, end_it);
        } else {
            for (; count > 0; --count) {
                push_back(value);
            }
        }
    }

    [[nodiscard]] constexpr allocator_type get_allocator() const noexcept
    {
        return alloc_;
    }

    [[nodiscard]] reference front() noexcept
    {
        GLOX_ASSERT(!empty());
        return backend_.front().value;
    }

    [[nodiscard]] const_reference front() const noexcept
    {
        GLOX_ASSERT(!empty());
        return backend_.front().value;
    }

    [[nodiscard]] reference back() noexcept
    {
        GLOX_ASSERT(!empty());
        return backend_.back().value;
    }

    [[nodiscard]] const_reference back() const noexcept
    {
        GLOX_ASSERT(!empty());
        return backend_.back().value;
    }

    [[nodiscard]] iterator begin() noexcept
    {
        return iterator(backend_.begin());
    }
    [[nodiscard]] const_iterator begin() const noexcept
    {
        return const_iterator(backend_.begin());
    }
    [[nodiscard]] const_iterator cbegin() const noexcept
    {
        return const_iterator(backend_.begin());
    }

    [[nodiscard]] iterator end() noexcept
    {
        return iterator(backend_.end());
    }
    [[nodiscard]] const_iterator end() const noexcept
    {
        return const_iterator(backend_.end());
    }
    [[nodiscard]] const_iterator cend() const noexcept
    {
        return const_iterator(backend_.end());
    }

    [[nodiscard]] reverse_iterator rbegin() noexcept
    {
        return reverse_iterator(end());
    }
    [[nodiscard]] const_reverse_iterator rbegin() const noexcept
    {
        return const_reverse_iterator(end());
    }
    [[nodiscard]] const_reverse_iterator crbegin() const noexcept
    {
        return const_reverse_iterator(cend());
    }

    [[nodiscard]] reverse_iterator rend() noexcept
    {
        return reverse_iterator(begin());
    }
    [[nodiscard]] const_reverse_iterator rend() const noexcept
    {
        return const_reverse_iterator(begin());
    }
    [[nodiscard]] const_reverse_iterator crend() const noexcept
    {
        return const_reverse_iterator(cbegin());
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return backend_.is_empty();
    }

    [[nodiscard]] size_type size() const noexcept
    {
        return backend_.size();
    }

    void clear() noexcept
    {
        while (!backend_.is_empty()) {
            node_t* n = static_cast<node_t*>(backend_.begin());
            backend_.erase(backend_.begin());
            destroy_node(n);
        }
    }

    template <typename... Args>
    reference emplace_back(Args&&... args)
    {
        node_t* n = create_node(FORWARD(args)...);
        backend_.push_back(n);
        return n->value;
    }

    void push_back(T const& value)
    {
        emplace_back(value);
    }

    void push_back(T&& value)
    {
        emplace_back(RVALUE(value));
    }

    template <typename... Args>
    reference emplace_front(Args&&... args)
    {
        node_t* n = create_node(FORWARD(args)...);
        backend_.push_front(n);
        return n->value;
    }

    void push_front(T const& value)
    {
        emplace_front(value);
    }

    void push_front(T&& value)
    {
        emplace_front(RVALUE(value));
    }

    void pop_front() noexcept
    {
        GLOX_ASSERT(!empty());
        node_t* n = static_cast<node_t*>(backend_.begin());
        backend_.erase(backend_.begin());
        destroy_node(n);
    }

    void pop_back() noexcept
    {
        GLOX_ASSERT(!empty());
        auto last = --backend_.end();
        node_t* n = static_cast<node_t*>(last);
        backend_.erase(last);
        destroy_node(n);
    }

    template <typename... Args>
    iterator emplace(const_iterator pos, Args&&... args)
    {
        node_t* n = create_node(FORWARD(args)...);
        auto mutIt = to_mutable_it(pos);
        return iterator(backend_.insert(mutIt, n));
    }

    iterator insert(const_iterator pos, T const& value)
    {
        return emplace(pos, value);
    }

    iterator insert(const_iterator pos, T&& value)
    {
        return emplace(pos, RVALUE(value));
    }

    iterator insert(const_iterator pos, size_type count, T const& value)
    {
        if (count == 0) {
            return iterator(to_mutable_it(pos));
        }
        iterator firstInserted = insert(pos, value);
        for (size_type i = 1; i < count; ++i) {
            insert(pos, value);
        }
        return firstInserted;
    }

    template <std::input_iterator InputIt>
    iterator insert(const_iterator pos, InputIt first, InputIt last)
    {
        if (first == last) {
            return iterator(to_mutable_it(pos));
        }
        iterator firstInserted = insert(pos, *first);
        ++first;
        for (; first != last; ++first) {
            insert(pos, *first);
        }
        return firstInserted;
    }

    iterator erase(const_iterator pos) noexcept
    {
        GLOX_ASSERT(pos != cend());
        auto mutIt = to_mutable_it(pos);
        node_t* n = static_cast<node_t*>(mutIt);
        auto nextIt = backend_.erase(mutIt);
        destroy_node(n);
        return iterator(nextIt);
    }

    iterator erase(const_iterator first, const_iterator last) noexcept
    {
        while (first != last) {
            first = erase(first);
        }
        return iterator(to_mutable_it(last));
    }

    void resize(size_type count)
    {
        while (size() > count) {
            pop_back();
        }
        while (size() < count) {
            emplace_back();
        }
    }

    void resize(size_type count, T const& value)
    {
        while (size() > count) {
            pop_back();
        }
        while (size() < count) {
            push_back(value);
        }
    }

    void swap(list& other) noexcept
    {
        if (this == &other) {
            return;
        }
        list tmp = RVALUE(*this);
        *this = RVALUE(other);
        other = RVALUE(tmp);
    }

    void splice(const_iterator pos, list& other) noexcept
    {
        if (this == &other || other.empty()) {
            return;
        }
        auto mutPos = to_mutable_it(pos);
        while (!other.backend_.is_empty()) {
            node_t* n = static_cast<node_t*>(other.backend_.begin());
            other.backend_.erase(other.backend_.begin());
            backend_.insert(mutPos, n);
        }
    }

    void splice(const_iterator pos, list&& other) noexcept
    {
        splice(pos, other);
    }

    void splice(const_iterator pos, list& other, const_iterator it) noexcept
    {
        auto mutIt = other.to_mutable_it(it);
        auto mutPos = to_mutable_it(pos);
        if (mutPos == mutIt || mutPos == mutIt.next()) {
            return;
        }
        node_t* n = static_cast<node_t*>(mutIt);
        other.backend_.erase(mutIt);
        backend_.insert(mutPos, n);
    }

    size_type remove(T const& value)
    {
        return remove_if([&](T const& item) { return item == value; });
    }

    template <typename Pred>
    size_type remove_if(Pred pred)
    {
        size_type removed = 0;
        for (auto it = begin(); it != end();) {
            if (pred(*it)) {
                it = erase(it);
                ++removed;
            } else {
                ++it;
            }
        }
        return removed;
    }

    size_type unique()
    {
        return unique([](T const& a, T const& b) { return a == b; });
    }

    template <typename BinaryPred>
    size_type unique(BinaryPred pred)
    {
        if (size() < 2) {
            return 0;
        }
        size_type removed = 0;
        auto prev = begin();
        auto cur = prev;
        ++cur;
        while (cur != end()) {
            if (pred(*prev, *cur)) {
                cur = erase(cur);
                ++removed;
            } else {
                prev = cur;
                ++cur;
            }
        }
        return removed;
    }

    void reverse() noexcept
    {
        if (size() < 2) {
            return;
        }
        backend_t reversed;
        while (!backend_.is_empty()) {
            node_t* n = static_cast<node_t*>(backend_.begin());
            backend_.erase(backend_.begin());
            reversed.push_front(n);
        }
        while (!reversed.is_empty()) {
            node_t* n = static_cast<node_t*>(reversed.begin());
            reversed.erase(reversed.begin());
            backend_.push_back(n);
        }
    }

    friend bool operator==(list const& lhs, list const& rhs)
    {
        if (lhs.size() != rhs.size()) {
            return false;
        }
        auto it1 = lhs.begin();
        auto it2 = rhs.begin();
        for (; it1 != lhs.end(); ++it1, ++it2) {
            if (!(*it1 == *it2)) {
                return false;
            }
        }
        return true;
    }

private:
    typename backend_t::iterator to_mutable_it(const_iterator pos) noexcept
    {
        node_t const* cnode = static_cast<node_t const*>(pos.it_);
        return typename backend_t::iterator(const_cast<node_t*>(cnode));
    }

    void steal_from(list& other) noexcept
    {
        while (!other.backend_.is_empty()) {
            node_t* n = static_cast<node_t*>(other.backend_.begin());
            other.backend_.erase(other.backend_.begin());
            backend_.push_back(n);
        }
    }
};

GLOX_END_EXPORT
} // namespace glox
