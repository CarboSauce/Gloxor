#pragma once

#ifndef USE_MODULES
#include "glox/tuple.hpp"
#include "util.hpp"
#include <atomic>
#include <concepts>
#include <cstdint>
#endif

namespace glox {
namespace detail {
    inline void cpu_relax() noexcept
    {
#if defined(__x86_64__) || defined(__i386__)
        __builtin_ia32_pause();
#elif defined(__aarch64__) || defined(__arm__)
        asm volatile("yield" ::: "memory");
#else
        asm volatile("" ::: "memory");
#endif
    }
} // namespace detail

GLOX_BEGIN_EXPORT

// concepts

template <typename T>
concept mutex_type = requires(T mutex) {
    { mutex.lock() };
    { mutex.unlock() };
};

template <typename T>
concept try_mutex_type = mutex_type<T> && requires(T mutex) {
    { mutex.try_lock() } -> std::convertible_to<bool>;
};

template <typename T>
concept shared_mutex_type = mutex_type<T> && requires(T mutex) {
    { mutex.lock_shared() };
    { mutex.unlock_shared() };
};

template <typename T>
concept try_shared_mutex_type
    = shared_mutex_type<T> && try_mutex_type<T> && requires(T mutex) {
          { mutex.try_lock_shared() } -> std::convertible_to<bool>;
      };

// lock tags

struct defer_lock_t
{
    explicit defer_lock_t() = default;
};
struct try_to_lock_t
{
    explicit try_to_lock_t() = default;
};
struct adopt_lock_t
{
    explicit adopt_lock_t() = default;
};

inline constexpr defer_lock_t defer_lock { };
inline constexpr try_to_lock_t try_to_lock { };
inline constexpr adopt_lock_t adopt_lock { };

// unique & shared movable raii wrappers

template <mutex_type T>
class unique_lock
{
    T* mutex = nullptr;
    bool owns = false;

public:
    unique_lock() noexcept = default;

    explicit unique_lock(T& m)
        : mutex(std::addressof(m))
        , owns(true)
    {
        mutex->lock();
    }

    unique_lock(T& m, defer_lock_t) noexcept
        : mutex(std::addressof(m))
        , owns(false)
    {
    }

    unique_lock(T& m, try_to_lock_t)
        requires try_mutex_type<T>
        : mutex(std::addressof(m))
        , owns(mutex->try_lock())
    {
    }

    unique_lock(T& m, adopt_lock_t) noexcept
        : mutex(std::addressof(m))
        , owns(true)
    {
    }

    ~unique_lock()
    {
        if (owns && mutex) {
            mutex->unlock();
        }
    }

    unique_lock(const unique_lock&) = delete;
    unique_lock& operator=(const unique_lock&) = delete;

    unique_lock(unique_lock&& other) noexcept
        : mutex(glox::exchange(other.mutex, nullptr))
        , owns(glox::exchange(other.owns, false))
    {
    }

    unique_lock& operator=(unique_lock&& other) noexcept
    {
        if (this != &other) {
            if (owns && mutex) {
                mutex->unlock();
            }
            mutex = glox::exchange(other.mutex, nullptr);
            owns = glox::exchange(other.owns, false);
        }
        return *this;
    }

    void lock()
    {
        if (mutex && !owns) {
            mutex->lock();
            owns = true;
        }
    }

    bool try_lock()
        requires try_mutex_type<T>
    {
        if (mutex && !owns) {
            owns = mutex->try_lock();
        }
        return owns;
    }

    void unlock()
    {
        if (mutex && owns) {
            mutex->unlock();
            owns = false;
        }
    }

    T* release() noexcept
    {
        owns = false;
        return glox::exchange(mutex, nullptr);
    }

    [[nodiscard]] bool owns_lock() const noexcept
    {
        return owns;
    }
    explicit operator bool() const noexcept
    {
        return owns;
    }
    [[nodiscard]] T* mutex_ptr() const noexcept
    {
        return mutex;
    }
};

template <shared_mutex_type T>
class shared_lock
{
    T* mutex = nullptr;
    bool owns = false;

public:
    shared_lock() noexcept = default;

    explicit shared_lock(T& m)
        : mutex(std::addressof(m))
        , owns(true)
    {
        mutex->lock_shared();
    }

    shared_lock(T& m, defer_lock_t) noexcept
        : mutex(std::addressof(m))
        , owns(false)
    {
    }

    shared_lock(T& m, try_to_lock_t)
        requires try_shared_mutex_type<T>
        : mutex(std::addressof(m))
        , owns(mutex->try_lock_shared())
    {
    }

    shared_lock(T& m, adopt_lock_t) noexcept
        : mutex(std::addressof(m))
        , owns(true)
    {
    }

    ~shared_lock()
    {
        if (owns and mutex) {
            mutex->unlock_shared();
        }
    }

    shared_lock(const shared_lock&) = delete;
    shared_lock& operator=(const shared_lock&) = delete;

    shared_lock(shared_lock&& other) noexcept
        : mutex(glox::exchange(other.mutex, nullptr))
        , owns(glox::exchange(other.owns, false))
    {
    }

    shared_lock& operator=(shared_lock&& other) noexcept
    {
        if (this != &other) {
            if (owns and mutex) {
                mutex->unlock_shared();
            }
            mutex = glox::exchange(other.mutex, nullptr);
            owns = glox::exchange(other.owns, false);
        }
        return *this;
    }

    void lock()
    {
        if (mutex && !owns) {
            mutex->lock_shared();
            owns = true;
        }
    }

    bool try_lock()
        requires try_shared_mutex_type<T>
    {
        if (mutex and not owns) {
            owns = mutex->try_lock_shared();
        }
        return owns;
    }

    void unlock()
    {
        if (mutex and owns) {
            mutex->unlock_shared();
            owns = false;
        }
    }

    T* release() noexcept
    {
        owns = false;
        return glox::exchange(mutex, nullptr);
    }

    [[nodiscard]] bool owns_lock() const noexcept
    {
        return owns;
    }
    explicit operator bool() const noexcept
    {
        return owns;
    }
    [[nodiscard]] T* mutex_ptr() const noexcept
    {
        return mutex;
    }
};

// deadlock-free multi-locking & reverse unlocking

GLOX_END_EXPORT
namespace detail {
    // Rotates the failed lock to the front of the pack so the next iteration
    // blocks on the contended lock first (matches libstdc++ __try_lock_impl /
    // __lock_impl)
    template <int Index, try_mutex_type L1>
    int try_lock_remaining(L1& l1)
    {
        if (l1.try_lock()) {
            return -1; // All succeeded
        }
        return Index; // Return index of lock that failed
    }

    template <
        int Index,
        try_mutex_type L1,
        try_mutex_type L2,
        try_mutex_type... L3
    >
    int try_lock_remaining(L1& l1, L2& l2, L3&... l3)
    {
        unique_lock<L1> guard(l1, try_to_lock);
        if (!guard.owns_lock()) {
            return Index;
        }
        const int failed = try_lock_remaining<Index + 1>(l2, l3...);
        if (failed == -1) {
            guard.release();
        }
        return failed; // If failed != -1, guard destructor automatically
                       // unlocks l1
    }

    // Locks element `Target` first (blocking), then tries all remaining
    // elements in circular order
    template <int Target, int Current = 0, typename Tuple, std::size_t... Is>
    int lock_from_pivot(Tuple& t, std::index_sequence<Is...>)
    {
        constexpr int n = static_cast<int>(sizeof...(Is));
        if constexpr (Current == Target) {
            auto& first = glox::get<Target>(t);
            using first_lock_t = std::remove_reference_t<decltype(first)>;
            unique_lock<first_lock_t> guard(
                first
            ); // Explicit type avoids copy CTAD

            auto try_rest = [&]<std::size_t... Offsets>(
                                std::index_sequence<Offsets...>
                            ) -> int {
                if constexpr (sizeof...(Offsets) == 0) {
                    return -1;
                } else {
                    return try_lock_remaining<1>(
                        glox::get<(Target + 1 + Offsets) % n>(t)...
                    );
                }
            };

            const int failed_offset
                = try_rest(std::make_index_sequence<n - 1> { });
            if (failed_offset == -1) {
                guard.release();
                return -1;
            }
            return (Target + failed_offset) % n;
        } else {
            return lock_from_pivot<Target, Current + 1>(
                t, std::index_sequence<Is...> { }
            );
        }
    }

    // Runtime dispatch over compile-time pivot index (switches directly to
    // concrete template instantiation)
    template <typename Tuple, std::size_t... Is>
    void lock_loop(Tuple& t, std::index_sequence<Is...> seq)
    {
        int pivot = 0;
        while (pivot != -1) {
            // Fold expression dispatches runtime `pivot` to compile-time
            // `lock_from_pivot<Is>`
            ((static_cast<int>(Is) == pivot
                     ? (pivot = lock_from_pivot<static_cast<int>(Is)>(t, seq),
                           true)
                     : false)
                || ...);
        }
    }
} // namespace detail

GLOX_BEGIN_EXPORT

inline void lock() noexcept { }

template <mutex_type L1>
void lock(L1& l1)
{
    l1.lock();
}

template <try_mutex_type L1, try_mutex_type L2, try_mutex_type... L3>
void lock(L1& l1, L2& l2, L3&... l3)
{
    if constexpr (std::same_as<L1, L2> && (std::same_as<L1, L3> && ...)) {
        constexpr int n = 2 + static_cast<int>(sizeof...(L3));
        unique_lock<L1> locks[]
            = { { l1, defer_lock }, { l2, defer_lock }, { l3, defer_lock }... };

        int first = 0;
        do {
            locks[first].lock();
            for (int j = 1; j < n; ++j) {
                const int idx = (first + j) % n;
                if (!locks[idx].try_lock()) {
                    for (int k = j; k != 0; --k) {
                        locks[(first + k - 1) % n].unlock();
                    }
                    first = idx;
                    break;
                }
            }
        } while (!locks[first].owns_lock());

        for (auto& l : locks) {
            l.release();
        }
    } else {
        glox::tuple<L1&, L2&, L3&...> t { l1, l2, l3... };
        detail::lock_loop(t, std::index_sequence_for<L1, L2, L3...> { });
    }
}

inline void unlock() noexcept { }

template <mutex_type L1>
void unlock(L1& l1) noexcept
{
    l1.unlock();
}

template <mutex_type L1, mutex_type L2, mutex_type... L3>
void unlock(L1& l1, L2& l2, L3&... l3) noexcept
{
    glox::unlock(l2, l3...);
    l1.unlock();
}

// non-movable raii guards

template <mutex_type T>
class lock_guard
{
    T& mutex;

public:
    explicit lock_guard(T& m)
        : mutex(m)
    {
        mutex.lock();
    }

    lock_guard(T& m, adopt_lock_t) noexcept
        : mutex(m)
    {
    }

    ~lock_guard()
    {
        mutex.unlock();
    }

    lock_guard(const lock_guard&) = delete;
    lock_guard& operator=(const lock_guard&) = delete;
};

template <mutex_type... Mutexes>
class scoped_lock
{
    glox::tuple<Mutexes&...> mutexes;

    template <std::size_t... Is>
    void unlock_impl(std::index_sequence<Is...>) noexcept
    {
        glox::unlock(glox::get<Is>(mutexes)...);
    }

public:
    explicit scoped_lock(Mutexes&... m)
        requires(try_mutex_type<Mutexes> && ...)
        : mutexes { m... }
    {
        glox::lock(m...);
    }

    explicit scoped_lock(adopt_lock_t, Mutexes&... m) noexcept
        : mutexes(m...)
    {
    }

    ~scoped_lock()
    {
        unlock_impl(std::index_sequence_for<Mutexes...> { });
    }

    scoped_lock(const scoped_lock&) = delete;
    scoped_lock& operator=(const scoped_lock&) = delete;
};

template <mutex_type Mutex>
class scoped_lock<Mutex>
{
    Mutex& mutex;

public:
    explicit scoped_lock(Mutex& m)
        : mutex(m)
    {
        mutex.lock();
    }

    explicit scoped_lock(adopt_lock_t, Mutex& m) noexcept
        : mutex(m)
    {
    }

    ~scoped_lock()
    {
        mutex.unlock();
    }

    scoped_lock(const scoped_lock&) = delete;
    scoped_lock& operator=(const scoped_lock&) = delete;
};

template <>
class scoped_lock<>
{
public:
    explicit scoped_lock() noexcept = default;
    explicit scoped_lock(adopt_lock_t) noexcept { }
    scoped_lock(const scoped_lock&) = delete;
    scoped_lock& operator=(const scoped_lock&) = delete;
};

// kernel-safe spinlock primitives

class spinlock
{
    std::atomic_flag flag = { };

public:
    spinlock() noexcept = default;
    spinlock(const spinlock&) = delete;
    spinlock& operator=(const spinlock&) = delete;

    void lock() noexcept
    {
        for (;;) {
            if (!flag.test_and_set(std::memory_order_acquire)) {
                return;
            }
            while (flag.test(std::memory_order_relaxed)) {
                detail::cpu_relax();
            }
        }
    }

    bool try_lock() noexcept
    {
        return !flag.test_and_set(std::memory_order_acquire);
    }

    void unlock() noexcept
    {
        flag.clear(std::memory_order_release);
    }
};

class fair_spinlock
{
    std::atomic<unsigned int> next_ticket { 0 };
    std::atomic<unsigned int> serving_ticket { 0 };

public:
    fair_spinlock() noexcept = default;
    fair_spinlock(const fair_spinlock&) = delete;
    fair_spinlock& operator=(const fair_spinlock&) = delete;

    void lock() noexcept
    {
        const unsigned int my_ticket
            = next_ticket.fetch_add(1, std::memory_order_relaxed);
        while (serving_ticket.load(std::memory_order_acquire) != my_ticket) {
            detail::cpu_relax();
        }
    }

    bool try_lock() noexcept
    {
        unsigned int current = serving_ticket.load(std::memory_order_relaxed);
        return next_ticket.compare_exchange_strong(
            current,
            current + 1,
            std::memory_order_acquire,
            std::memory_order_relaxed
        );
    }

    void unlock() noexcept
    {
        serving_ticket.fetch_add(1, std::memory_order_release);
    }
};

class spin_rwlock
{
    static constexpr std::uint32_t writer_active = 1u << 31;
    static constexpr std::uint32_t writer_waiting = 1u << 30;
    static constexpr std::uint32_t reader_mask
        = ~(writer_active | writer_waiting);

    std::atomic<std::uint32_t> state { 0 };

public:
    spin_rwlock() noexcept = default;
    spin_rwlock(const spin_rwlock&) = delete;
    spin_rwlock& operator=(const spin_rwlock&) = delete;

    void lock() noexcept
    {
        std::uint32_t expected = state.load(std::memory_order_relaxed);
        for (;;) {
            if ((expected & (writer_active | reader_mask)) == 0) {
                if (state.compare_exchange_weak(
                        expected,
                        writer_active,
                        std::memory_order_acquire,
                        std::memory_order_relaxed
                    )) {
                    return;
                }
            } else {
                if ((expected & writer_waiting) == 0) {
                    state.compare_exchange_weak(
                        expected,
                        expected | writer_waiting,
                        std::memory_order_relaxed,
                        std::memory_order_relaxed
                    );
                }
                detail::cpu_relax();
                expected = state.load(std::memory_order_relaxed);
            }
        }
    }

    bool try_lock() noexcept
    {
        std::uint32_t expected = 0;
        return state.compare_exchange_strong(
            expected,
            writer_active,
            std::memory_order_acquire,
            std::memory_order_relaxed
        );
    }

    void unlock() noexcept
    {
        state.fetch_and(~writer_active, std::memory_order_release);
    }

    void lock_shared() noexcept
    {
        std::uint32_t expected = state.load(std::memory_order_relaxed);
        for (;;) {
            if ((expected & (writer_active | writer_waiting)) != 0
                || (expected & reader_mask) == reader_mask) {
                detail::cpu_relax();
                expected = state.load(std::memory_order_relaxed);
                continue;
            }

            if (state.compare_exchange_weak(
                    expected,
                    expected + 1,
                    std::memory_order_acquire,
                    std::memory_order_relaxed
                )) {
                return;
            }
        }
    }

    bool try_lock_shared() noexcept
    {
        std::uint32_t expected = state.load(std::memory_order_relaxed);
        while ((expected & (writer_active | writer_waiting)) == 0
               && (expected & reader_mask) < reader_mask) {
            if (state.compare_exchange_weak(
                    expected,
                    expected + 1,
                    std::memory_order_acquire,
                    std::memory_order_relaxed
                )) {
                return true;
            }
        }
        return false;
    }

    void unlock_shared() noexcept
    {
        state.fetch_sub(1, std::memory_order_release);
    }
};

// Kernel-friendly recursive spinlock parameterized by a non-zero owner ID
// provider (e.g., returning current CPU/core ID + 1 or current task pointer as
// uintptr_t; 0 = unlocked)
template <auto GetOwnerId>
    requires requires {
        { GetOwnerId() } -> std::integral;
    }
class basic_recursive_spinlock
{
    using id_type = decltype(GetOwnerId());
    std::atomic<id_type> owner { 0 };
    std::uint32_t recursion_count = 0;

public:
    basic_recursive_spinlock() noexcept = default;
    basic_recursive_spinlock(const basic_recursive_spinlock&) = delete;
    basic_recursive_spinlock& operator=(const basic_recursive_spinlock&)
        = delete;

    void lock() noexcept
    {
        const auto self = GetOwnerId();
        if (owner.load(std::memory_order_relaxed) == self) {
            ++recursion_count;
            return;
        }

        std::uintptr_t expected = 0;
        while (!owner.compare_exchange_weak(
            expected, self, std::memory_order_acquire, std::memory_order_relaxed
        )) {
            expected = 0;
            detail::cpu_relax();
        }
        recursion_count = 1;
    }

    bool try_lock() noexcept
    {
        const auto self = GetOwnerId();
        if (owner.load(std::memory_order_relaxed) == self) {
            ++recursion_count;
            return true;
        }

        id_type expected = 0;
        if (owner.compare_exchange_strong(
                expected,
                self,
                std::memory_order_acquire,
                std::memory_order_relaxed
            )) {
            recursion_count = 1;
            return true;
        }
        return false;
    }

    void unlock() noexcept
    {
        if (--recursion_count == 0) {
            owner.store(0, std::memory_order_release);
        }
    }
};

class seqlock
{
    std::atomic<std::uint64_t> seq { 0 };

public:
    seqlock() noexcept = default;
    seqlock(const seqlock&) = delete;
    seqlock& operator=(const seqlock&) = delete;

    void lock() noexcept
    {
        std::uint64_t expected = seq.load(std::memory_order_relaxed);
        for (;;) {
            if ((expected & 1u) == 0) {
                if (seq.compare_exchange_weak(
                        expected,
                        expected + 1,
                        std::memory_order_acquire,
                        std::memory_order_relaxed
                    )) {
                    return;
                }
            } else {
                detail::cpu_relax();
                expected = seq.load(std::memory_order_relaxed);
            }
        }
    }

    bool try_lock() noexcept
    {
        std::uint64_t expected = seq.load(std::memory_order_relaxed);
        return ((expected & 1u) == 0)
            && seq.compare_exchange_strong(
                expected,
                expected + 1,
                std::memory_order_acquire,
                std::memory_order_relaxed
            );
    }

    void unlock() noexcept
    {
        seq.fetch_add(1, std::memory_order_release);
    }

    [[nodiscard]] std::uint64_t read_begin() const noexcept
    {
        std::uint64_t s = seq.load(std::memory_order_acquire);
        while ((s & 1u) != 0) {
            detail::cpu_relax();
            s = seq.load(std::memory_order_acquire);
        }
        return s;
    }

    [[nodiscard]] bool read_retry(std::uint64_t start_seq) const noexcept
    {
        std::atomic_thread_fence(std::memory_order_acquire);
        return seq.load(std::memory_order_relaxed) != start_seq;
    }
};

template <mutex_type T>
unique_lock(T) -> unique_lock<T>;
template <mutex_type... Ts>
scoped_lock(Ts...) -> scoped_lock<Ts...>;

GLOX_END_EXPORT
} // namespace glox
