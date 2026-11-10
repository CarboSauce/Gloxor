#pragma once

#ifndef USE_MODULES
#include <atomic>
#include <concepts>
#endif

namespace glox {
GLOX_BEGIN_EXPORT
template <typename T>
concept mutex_type = requires(T mutex) {
    { mutex.lock() };
    { mutex.unlock() };
};

template <typename T>
class lock_guard
{
    T& mutex;

public:
    lock_guard(T& mutex)
        : mutex(mutex)
    {
        mutex.lock();
    }
    ~lock_guard()
    {
        mutex.unlock();
    }
    lock_guard(const lock_guard&) = delete;
    lock_guard& operator=(const lock_guard&) = delete;
};
template <typename T>
class scoped_lock
{
    T mutex;

public:
    scoped_lock()
    {
        mutex.lock();
    }
    ~scoped_lock()
    {
        mutex.unlock();
    }
    scoped_lock(const scoped_lock&) = delete;
    scoped_lock& operator=(const scoped_lock&) = delete;
};

class spinlock
{
    std::atomic_flag flag = ATOMIC_FLAG_INIT;

public:
    spinlock() noexcept = default;

    spinlock(const spinlock&) = delete;
    spinlock& operator=(const spinlock&) = delete;

    void lock() noexcept
    {
        while (flag.test_and_set(std::memory_order_acquire)) {
#if defined(__x86_64__) || defined(__i386__)
            asm volatile("pause" ::: "memory");
#elif defined(__aarch64__) || defined(__arm__)
            asm volatile("isb" ::: "memory");
#else
            std::this_thread::yield();
#endif
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
        const unsigned int myTicket
            = next_ticket.fetch_add(1, std::memory_order_relaxed);
        while (serving_ticket.load(std::memory_order_acquire) != myTicket) {
#if defined(__x86_64__) || defined(__i386__)
            __builtin_ia32_pause();
#elif defined(__aarch64__) || defined(__arm__)
            asm volatile("isb" ::: "memory");
#else
            ;
#endif
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

GLOX_END_EXPORT
} // namespace glox
