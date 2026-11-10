#pragma once

#ifndef USE_MODULES
#include "alloc.hpp"
#include "box.hpp"
#include "glox/array.hpp"
#include "option.hpp"
#include <atomic>
#include <cstddef>
#include <new>
#include <utility>
#endif

namespace glox {
GLOX_BEGIN_EXPORT

template <typename T, std::size_t Capacity>
class circular_buffer
{
    static_assert(Capacity > 0, "Capacity must be greater than zero");

    alignas(T) unsigned char storage_[Capacity * sizeof(T)];
    std::size_t head_ = 0;
    std::size_t tail_ = 0;
    std::size_t size_ = 0;

    T* ptr_at(std::size_t idx) noexcept
    {
        return reinterpret_cast<T*>(&storage_[idx * sizeof(T)]);
    }

    const T* ptr_at(std::size_t idx) const noexcept
    {
        return reinterpret_cast<const T*>(&storage_[idx * sizeof(T)]);
    }

public:
    constexpr circular_buffer() noexcept = default;

    ~circular_buffer()
    {
        clear();
    }

    circular_buffer(const circular_buffer& other)
    {
        std::size_t idx = other.head_;
        for (std::size_t i = 0; i < other.size_; ++i) {
            push(*other.ptr_at(idx));
            idx = (idx + 1) % Capacity;
        }
    }

    circular_buffer& operator=(const circular_buffer& other)
    {
        if (this != &other) {
            clear();
            std::size_t idx = other.head_;
            for (std::size_t i = 0; i < other.size_; ++i) {
                push(*other.ptr_at(idx));
                idx = (idx + 1) % Capacity;
            }
        }
        return *this;
    }

    circular_buffer(circular_buffer&& other) noexcept
    {
        while (!other.empty()) {
            push(std::move(*other.ptr_at(other.head_)));
            other.pop_front();
        }
    }

    circular_buffer& operator=(circular_buffer&& other) noexcept
    {
        if (this != &other) {
            clear();
            while (!other.empty()) {
                push(std::move(*other.ptr_at(other.head_)));
                other.pop_front();
            }
        }
        return *this;
    }

    template <typename... Args>
    bool emplace(Args&&... args)
    {
        if (full()) {
            return false;
        }
        ::new (static_cast<void*>(ptr_at(tail_)))
            T(std::forward<Args>(args)...);
        tail_ = (tail_ + 1) % Capacity;
        ++size_;
        return true;
    }

    bool push(const T& value)
    {
        return emplace(value);
    }
    bool push(T&& value)
    {
        return emplace(std::move(value));
    }

    void pop_front()
    {
        if (empty())
            return;
        ptr_at(head_)->~T();
        head_ = (head_ + 1) % Capacity;
        --size_;
    }

    glox::option<T> pop()
    {
        if (empty()) {
            return { };
        }
        T val = std::move(*ptr_at(head_));
        ptr_at(head_)->~T();
        head_ = (head_ + 1) % Capacity;
        --size_;
        return val;
    }

    T& front() noexcept
    {
        return *ptr_at(head_);
    }
    const T& front() const noexcept
    {
        return *ptr_at(head_);
    }

    T& back() noexcept
    {
        return *ptr_at((tail_ + Capacity - 1) % Capacity);
    }
    const T& back() const noexcept
    {
        return *ptr_at((tail_ + Capacity - 1) % Capacity);
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return size_ == 0;
    }
    [[nodiscard]] bool full() const noexcept
    {
        return size_ == Capacity;
    }
    [[nodiscard]] std::size_t size() const noexcept
    {
        return size_;
    }
    [[nodiscard]] constexpr std::size_t capacity() const noexcept
    {
        return Capacity;
    }

    void clear() noexcept
    {
        while (!empty()) {
            pop_front();
        }
    }
};

template <typename T, std::size_t Capacity>
class atomic_circular_buffer
{
    static_assert(Capacity > 0, "Capacity must be greater than zero");

    // Allocate space for Capacity + 1 slots to differentiate full vs empty
    // without extra variables
    static constexpr std::size_t REAL_CAPACITY = Capacity + 1;

    alignas(T) unsigned char storage_[REAL_CAPACITY * sizeof(T)];

    // Cache line padding (typically 64 bytes) prevents false sharing between
    // Producer and Consumer
    alignas(
        std::hardware_destructive_interference_size
    ) std::atomic<std::size_t> head_ { 0 };
    alignas(
        std::hardware_destructive_interference_size
    ) std::atomic<std::size_t> tail_ { 0 };

    T* ptr_at(std::size_t idx) noexcept
    {
        return reinterpret_cast<T*>(&storage_[idx * sizeof(T)]);
    }

    const T* ptr_at(std::size_t idx) const noexcept
    {
        return reinterpret_cast<const T*>(&storage_[idx * sizeof(T)]);
    }

public:
    atomic_circular_buffer() noexcept = default;

    ~atomic_circular_buffer()
    {
        std::size_t h = head_.load(std::memory_order_relaxed);
        const std::size_t t = tail_.load(std::memory_order_relaxed);
        while (h != t) {
            ptr_at(h)->~T();
            h = (h + 1) % REAL_CAPACITY;
        }
    }

    atomic_circular_buffer(const atomic_circular_buffer&) = delete;
    atomic_circular_buffer& operator=(const atomic_circular_buffer&) = delete;

    template <typename... Args>
    bool emplace(Args&&... args)
    {
        const std::size_t currentTail = tail_.load(std::memory_order_relaxed);
        const std::size_t nextTail = (currentTail + 1) % REAL_CAPACITY;

        // Producer reads head with acquire semantics to check if buffer is full
        if (nextTail == head_.load(std::memory_order_acquire)) {
            return false; // Buffer full
        }

        ::new (static_cast<void*>(ptr_at(currentTail)))
            T(std::forward<Args>(args)...);
        // Publish writes with release semantics
        tail_.store(nextTail, std::memory_order_release);
        return true;
    }

    bool push(const T& value)
    {
        return emplace(value);
    }
    bool push(T&& value)
    {
        return emplace(std::move(value));
    }

    glox::option<T> pop()
    {
        const std::size_t currentHead = head_.load(std::memory_order_relaxed);

        // Consumer reads tail with acquire semantics to check if buffer is
        // empty
        if (currentHead == tail_.load(std::memory_order_acquire)) {
            return { }; // Buffer empty
        }

        T val = std::move(*ptr_at(currentHead));
        ptr_at(currentHead)->~T();

        const std::size_t nextHead = (currentHead + 1) % REAL_CAPACITY;
        // Publish read slot freed with release semantics
        head_.store(nextHead, std::memory_order_release);
        return val;
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return head_.load(std::memory_order_relaxed)
            == tail_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] bool full() const noexcept
    {
        const std::size_t nextTail
            = (tail_.load(std::memory_order_relaxed) + 1) % REAL_CAPACITY;
        return nextTail == head_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] std::size_t size() const noexcept
    {
        const std::size_t h = head_.load(std::memory_order_relaxed);
        const std::size_t t = tail_.load(std::memory_order_relaxed);
        if (t >= h)
            return t - h;
        return REAL_CAPACITY + t - h;
    }

    [[nodiscard]] constexpr std::size_t capacity() const noexcept
    {
        return Capacity;
    }
};

template <typename T, glox::allocator<T> Allocator = glox::default_allocator<T>>
class spsq_circular_buffer
{
    std::size_t real_capacity_;
    std::size_t capacity_;

    glox::box<unsigned char[], Allocator> storage_;

    alignas(64) std::atomic<std::size_t> head_ { 0 };
    alignas(64) std::atomic<std::size_t> tail_ { 0 };

    T* ptr_at(std::size_t idx) noexcept
    {
        return reinterpret_cast<T*>(storage_.get_ptr() + (idx * sizeof(T)));
    }

    const T* ptr_at(std::size_t idx) const noexcept
    {
        return reinterpret_cast<const T*>(
            storage_.get_ptr() + (idx * sizeof(T))
        );
    }

    void destroy_active_elements() noexcept
    {
        if (!storage_)
            return;
        std::size_t h = head_.load(std::memory_order_relaxed);
        const std::size_t t = tail_.load(std::memory_order_relaxed);
        while (h != t) {
            ptr_at(h)->~T();
            h = (h + 1) % real_capacity_;
        }
    }

public:
    using value_type = T;
    using size_type = std::size_t;

    explicit spsq_circular_buffer(std::size_t capacity)
        : real_capacity_(capacity + 1)
        , capacity_(capacity)
        , storage_(real_capacity_ * sizeof(T))
    {
    }

    ~spsq_circular_buffer()
    {
        destroy_active_elements();
    }

    spsq_circular_buffer(const spsq_circular_buffer&) = delete;
    spsq_circular_buffer& operator=(const spsq_circular_buffer&) = delete;

    spsq_circular_buffer(spsq_circular_buffer&& other) noexcept
        : real_capacity_(glox::exchange(other.real_capacity_, 0))
        , capacity_(glox::exchange(other.capacity_, 0))
        , storage_(RVALUE(other.storage_))
        , head_(other.head_.load(std::memory_order_relaxed))
        , tail_(other.tail_.load(std::memory_order_relaxed))
    {
        other.head_.store(0, std::memory_order_relaxed);
        other.tail_.store(0, std::memory_order_relaxed);
    }

    spsq_circular_buffer& operator=(spsq_circular_buffer&& other) noexcept
    {
        if (this != &other) {
            destroy_active_elements();
            real_capacity_ = glox::exchange(other.real_capacity_, 0);
            capacity_ = glox::exchange(other.capacity_, 0);
            storage_ = RVALUE(other.storage_);
            head_.store(
                other.head_.load(std::memory_order_relaxed),
                std::memory_order_relaxed
            );
            tail_.store(
                other.tail_.load(std::memory_order_relaxed),
                std::memory_order_relaxed
            );
            other.head_.store(0, std::memory_order_relaxed);
            other.tail_.store(0, std::memory_order_relaxed);
        }
        return *this;
    }

    static glox::option<spsq_circular_buffer> create(std::size_t capacity)
    {
        if (capacity == 0) {
            return { };
        }
        spsq_circular_buffer buf(capacity);
        if (buf.storage_.is_null()) {
            return { };
        }
        return buf;
    }

    template <typename... Args>
    bool emplace(Args&&... args)
    {
        const std::size_t currentTail = tail_.load(std::memory_order_relaxed);
        const std::size_t nextTail = (currentTail + 1) % real_capacity_;

        if (nextTail == head_.load(std::memory_order_acquire)) {
            return false; // Buffer full
        }

        ::new (static_cast<void*>(ptr_at(currentTail))) T(FORWARD(args)...);
        tail_.store(nextTail, std::memory_order_release);
        return true;
    }

    bool push(const T& value)
    {
        return emplace(value);
    }
    bool push(T&& value)
    {
        return emplace(RVALUE(value));
    }

    glox::option<T> pop()
    {
        const std::size_t currentHead = head_.load(std::memory_order_relaxed);

        if (currentHead == tail_.load(std::memory_order_acquire)) {
            return { }; // Buffer empty
        }

        T val = RVALUE(*ptr_at(currentHead));
        ptr_at(currentHead)->~T();

        const std::size_t nextHead = (currentHead + 1) % real_capacity_;
        head_.store(nextHead, std::memory_order_release);
        return val;
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return head_.load(std::memory_order_relaxed)
            == tail_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] bool full() const noexcept
    {
        const std::size_t nextTail
            = (tail_.load(std::memory_order_relaxed) + 1) % real_capacity_;
        return nextTail == head_.load(std::memory_order_acquire);
    }

    [[nodiscard]] std::size_t size() const noexcept
    {
        const std::size_t h = head_.load(std::memory_order_relaxed);
        const std::size_t t = tail_.load(std::memory_order_relaxed);
        if (t >= h)
            return t - h;
        return real_capacity_ + t - h;
    }

    [[nodiscard]] std::size_t capacity() const noexcept
    {
        return capacity_;
    }
};
GLOX_END_EXPORT
} // namespace glox
