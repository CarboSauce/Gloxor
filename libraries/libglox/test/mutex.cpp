#ifndef USE_MODULES
#include "glox/mutex.hpp"
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#else
import std;
import glox;
#endif
#include "doctest.h"
#include "glox/macros.hpp"

using namespace glox;

// Static Concept Checks

static_assert(mutex_type<spinlock>, "spinlock must satisfy glox::mutex_type");
static_assert(
    try_mutex_type<spinlock>,
    "spinlock must satisfy glox::try_mutex_type"
);

static_assert(
    mutex_type<fair_spinlock>,
    "fair_spinlock must satisfy glox::mutex_type"
);
static_assert(
    try_mutex_type<fair_spinlock>,
    "fair_spinlock must satisfy glox::try_mutex_type"
);

static_assert(
    shared_mutex_type<spin_rwlock>,
    "spin_rwlock must satisfy glox::shared_mutex_type"
);
static_assert(
    try_shared_mutex_type<spin_rwlock>,
    "spin_rwlock must satisfy glox::try_shared_mutex_type"
);

static_assert(mutex_type<seqlock>, "seqlock must satisfy glox::mutex_type");
static_assert(
    try_mutex_type<seqlock>,
    "seqlock must satisfy glox::try_mutex_type"
);

static_assert(
    try_mutex_type<unique_lock<spinlock>>,
    "unique_lock must satisfy glox::try_mutex_type"
);
static_assert(
    try_mutex_type<shared_lock<spin_rwlock>>,
    "shared_lock must satisfy glox::try_mutex_type"
);

// Spinlock & Fair Spinlock Tests

TEST_CASE("Spinlock lock and unlock")
{
    spinlock lock;
    lock.lock();
    lock.unlock();
}

TEST_CASE("Spinlock try_lock behavior")
{
    spinlock lock;

    REQUIRE(lock.try_lock() == true);
    REQUIRE(lock.try_lock() == false);
    lock.unlock();

    REQUIRE(lock.try_lock() == true);
    lock.unlock();
}

TEST_CASE("Spinlock works with lock_guard")
{
    spinlock lock;
    int value = 0;
    {
        lock_guard<spinlock> guard(lock);
        value = 42;
    }
    REQUIRE(value == 42);
}

TEST_CASE("Spinlock multi-threaded mutual exclusion")
{
    spinlock lock;
    constexpr int num_threads = 8;
    constexpr int increments_per_thread = 20'000;
    int shared_counter = 0;

    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < increments_per_thread; ++j) {
                lock_guard<spinlock> guard(lock);
                ++shared_counter;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    REQUIRE(shared_counter == num_threads * increments_per_thread);
}

TEST_CASE("Fair spinlock lock and unlock")
{
    fair_spinlock lock;
    lock.lock();
    lock.unlock();
}

TEST_CASE("Fair spinlock try_lock behavior")
{
    fair_spinlock lock;

    REQUIRE(lock.try_lock() == true);
    REQUIRE(lock.try_lock() == false);
    lock.unlock();

    REQUIRE(lock.try_lock() == true);
    lock.unlock();
}

TEST_CASE("Fair spinlock works with lock_guard")
{
    fair_spinlock lock;
    int value = 0;
    {
        lock_guard<fair_spinlock> guard(lock);
        value = 100;
    }
    REQUIRE(value == 100);
}

TEST_CASE("Fair spinlock multi-threaded mutual exclusion")
{
    fair_spinlock lock;
    constexpr int num_threads = 8;
    constexpr int increments_per_thread = 20'000;
    int shared_counter = 0;

    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < increments_per_thread; ++j) {
                lock_guard<fair_spinlock> guard(lock);
                ++shared_counter;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    REQUIRE(shared_counter == num_threads * increments_per_thread);
}

TEST_CASE("Fair spinlock FIFO queue ordering")
{
    fair_spinlock lock;
    constexpr int num_threads = 6;
    std::vector<int> execution_order;
    execution_order.reserve(num_threads);

    std::atomic<bool> start_signal { false };
    std::atomic<int> ready_count { 0 };

    lock.lock();

    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, id = i]() {
            ++ready_count;
            while (!start_signal.load(std::memory_order_relaxed)) {
                std::this_thread::yield();
            }

            lock.lock();
            execution_order.push_back(id);
            std::this_thread::sleep_for(std::chrono::microseconds(50));
            lock.unlock();
        });
    }

    while (ready_count.load() < num_threads) {
        std::this_thread::yield();
    }

    start_signal.store(true);
    lock.unlock();

    for (auto& t : threads) {
        t.join();
    }

    REQUIRE(execution_order.size() == static_cast<std::size_t>(num_threads));
}

// spin_rwlock & shared_lock Tests

TEST_CASE("spin_rwlock shared and exclusive behavior")
{
    spin_rwlock rw;

    // Multiple readers can acquire concurrently
    REQUIRE(rw.try_lock_shared() == true);
    REQUIRE(rw.try_lock_shared() == true);

    // Exclusive writer must fail while readers hold lock
    REQUIRE(rw.try_lock() == false);

    rw.unlock_shared();
    REQUIRE(rw.try_lock() == false);

    rw.unlock_shared();

    // Now writer can acquire
    REQUIRE(rw.try_lock() == true);
    REQUIRE(rw.try_lock() == false);
    REQUIRE(rw.try_lock_shared() == false);
    rw.unlock();

    // Readers can acquire again
    REQUIRE(rw.try_lock_shared() == true);
    rw.unlock_shared();
}

TEST_CASE("shared_lock RAII, tags, and move semantics")
{
    spin_rwlock rw;

    SUBCASE("Basic RAII")
    {
        {
            shared_lock<spin_rwlock> s1(rw);
            shared_lock<spin_rwlock> s2(rw);
            REQUIRE(s1.owns_lock() == true);
            REQUIRE(s2.owns_lock() == true);
            REQUIRE(rw.try_lock() == false);
        }
        REQUIRE(rw.try_lock() == true);
        rw.unlock();
    }

    SUBCASE("defer_lock, try_to_lock, adopt_lock")
    {
        shared_lock<spin_rwlock> deferred(rw, defer_lock);
        REQUIRE(deferred.owns_lock() == false);
        deferred.lock();
        REQUIRE(deferred.owns_lock() == true);

        shared_lock<spin_rwlock> tried(rw, try_to_lock);
        REQUIRE(tried.owns_lock() == true);

        rw.lock_shared();
        shared_lock<spin_rwlock> adopted(rw, adopt_lock);
        REQUIRE(adopted.owns_lock() == true);
    }

    SUBCASE("Move construction and assignment")
    {
        shared_lock<spin_rwlock> s1(rw);
        REQUIRE(s1.owns_lock() == true);

        shared_lock<spin_rwlock> s2(std::move(s1));
        REQUIRE(s1.owns_lock() == false);
        REQUIRE(s1.mutex_ptr() == nullptr);
        REQUIRE(s2.owns_lock() == true);

        shared_lock<spin_rwlock> s3;
        s3 = std::move(s2);
        REQUIRE(s2.owns_lock() == false);
        REQUIRE(s3.owns_lock() == true);
    }
}

TEST_CASE("spin_rwlock multi-threaded readers and writers")
{
    spin_rwlock rw;
    constexpr int num_writers = 3;
    constexpr int num_readers = 6;
    constexpr int write_iterations = 5'000;

    std::atomic<bool> done { false };
    std::atomic<int> active_readers { 0 };
    std::atomic<int> active_writers { 0 };
    std::atomic<bool> invariant_violated { false };
    int shared_a = 0;
    int shared_b = 0;

    std::vector<std::thread> threads;

    for (int i = 0; i < num_writers; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < write_iterations; ++j) {
                unique_lock<spin_rwlock> wlock(rw);
                int const w = ++active_writers;
                if (w != 1
                    || active_readers.load(std::memory_order_acquire) != 0) {
                    invariant_violated.store(true, std::memory_order_relaxed);
                }
                ++shared_a;
                ++shared_b;
                --active_writers;
            }
        });
    }

    for (int i = 0; i < num_readers; ++i) {
        threads.emplace_back([&]() {
            while (!done.load(std::memory_order_acquire)) {
                shared_lock<spin_rwlock> rlock(rw);
                ++active_readers;
                if (active_writers.load(std::memory_order_acquire) != 0
                    || shared_a != shared_b) {
                    invariant_violated.store(true, std::memory_order_relaxed);
                }
                --active_readers;
            }
        });
    }

    for (int i = 0; i < num_writers; ++i) {
        threads[i].join();
    }
    done.store(true, std::memory_order_release);
    for (int i = num_writers; i < num_writers + num_readers; ++i) {
        threads[i].join();
    }

    REQUIRE(invariant_violated.load() == false);
    REQUIRE(shared_a == num_writers * write_iterations);
    REQUIRE(shared_b == num_writers * write_iterations);
}

// glox::lock, glox::unlock, and glox::scoped_lock Tests

namespace {
struct tracked_mutex
{
    spinlock inner;
    std::vector<int>* log = nullptr;
    int id = 0;

    void lock()
    {
        inner.lock();
        if (log) {
            log->push_back(id);
        }
    }

    bool try_lock()
    {
        if (inner.try_lock()) {
            if (log) {
                log->push_back(id);
            }
            return true;
        }
        return false;
    }

    void unlock()
    {
        if (log) {
            log->push_back(-id);
        }
        inner.unlock();
    }
};
} // namespace

TEST_CASE("glox::unlock unlocks in exact reverse argument order")
{
    std::vector<int> events;
    tracked_mutex m1 { { }, &events, 1 };
    tracked_mutex m2 { { }, &events, 2 };
    tracked_mutex m3 { { }, &events, 3 };

    glox::lock(m1, m2, m3);
    REQUIRE(events == std::vector<int> { 1, 2, 3 });

    events.clear();
    glox::unlock(m1, m2, m3);
    REQUIRE(events == std::vector<int> { -3, -2, -1 });
}

TEST_CASE("glox::scoped_lock unlocks in reverse order")
{
    std::vector<int> events;
    tracked_mutex m1 { { }, &events, 1 };
    tracked_mutex m2 { { }, &events, 2 };
    tracked_mutex m3 { { }, &events, 3 };

    {
        scoped_lock guard(m1, m2, m3);
        REQUIRE(events == std::vector<int> { 1, 2, 3 });
        events.clear();
    }

    REQUIRE(events == std::vector<int> { -3, -2, -1 });
}

TEST_CASE("glox::lock deadlock avoidance under opposing lock orders")
{
    spinlock m1;
    spinlock m2;
    fair_spinlock m3; // Heterogeneous mutex type to exercise both lock paths
    constexpr int iterations = 15'000;
    int shared_sum = 0;

    // Thread A locks (m1, m2, m3); Thread B locks in opposite order (m3, m2,
    // m1)
    std::thread t1([&]() {
        for (int i = 0; i < iterations; ++i) {
            scoped_lock guard(m1, m2, m3);
            ++shared_sum;
        }
    });

    std::thread t2([&]() {
        for (int i = 0; i < iterations; ++i) {
            scoped_lock guard(m3, m2, m1);
            ++shared_sum;
        }
    });

    t1.join();
    t2.join();

    REQUIRE(shared_sum == iterations * 2);
}

TEST_CASE("glox::lock mixed shared_lock and unique_lock deadlock avoidance")
{
    spin_rwlock rw1;
    spin_rwlock rw2;
    constexpr int iterations = 10'000;
    int value1 = 0;
    int value2 = 0;

    std::thread writer([&]() {
        for (int i = 0; i < iterations; ++i) {
            unique_lock<spin_rwlock> l1(rw1, defer_lock);
            unique_lock<spin_rwlock> l2(rw2, defer_lock);
            scoped_lock guard(l1, l2);
            ++value1;
            ++value2;
        }
    });

    std::thread reader_writer([&]() {
        for (int i = 0; i < iterations; ++i) {
            shared_lock<spin_rwlock> l2(rw2, defer_lock);
            unique_lock<spin_rwlock> l1(rw1, defer_lock);
            scoped_lock guard(l2, l1); // Opposite order + mixed lock modes
            value1 += (value2 >= 0 ? 1 : 0);
        }
    });

    writer.join();
    reader_writer.join();

    REQUIRE(value1 == iterations * 2);
    REQUIRE(value2 == iterations);
}

// basic_recursive_spinlock & seqlock Tests

namespace {
inline std::atomic<std::uintptr_t> mock_cpu_id { 1 };
std::uintptr_t current_test_owner_id() noexcept
{
    return mock_cpu_id.load(std::memory_order_relaxed);
}
} // namespace

TEST_CASE("basic_recursive_spinlock re-entrancy and contention")
{
    basic_recursive_spinlock<current_test_owner_id> rlock;

    mock_cpu_id.store(1);
    rlock.lock();
    REQUIRE(rlock.try_lock() == true); // Same owner succeeds recursively

    // Simulate a different CPU/core trying to acquire
    mock_cpu_id.store(2);
    REQUIRE(rlock.try_lock() == false);

    // Switch back to owner 1 and unwind both acquisitions
    mock_cpu_id.store(1);
    rlock.unlock();

    mock_cpu_id.store(2);
    REQUIRE(rlock.try_lock() == false); // Still held at depth 1

    mock_cpu_id.store(1);
    rlock.unlock();

    mock_cpu_id.store(2);
    REQUIRE(rlock.try_lock() == true); // Now released
    rlock.unlock();
}

TEST_CASE("seqlock optimistic reads and consistent snapshots")
{
    seqlock sl;
    struct payload
    {
        std::uint64_t a = 0;
        std::uint64_t b = 0;
    } data;

    constexpr int iterations = 20'000;
    std::atomic<bool> done { false };
    std::atomic<bool> torn_read { false };

    std::thread writer([&]() {
        for (int i = 1; i <= iterations; ++i) {
            lock_guard<seqlock> guard(sl);
            data.a = static_cast<std::uint64_t>(i);
            data.b = static_cast<std::uint64_t>(i) * 10;
        }
        done.store(true, std::memory_order_release);
    });

    std::thread reader([&]() {
        while (!done.load(std::memory_order_acquire)) {
            payload snap;
            std::uint64_t seq;
            do {
                seq = sl.read_begin();
                snap = data;
            } while (sl.read_retry(seq));

            if (snap.b != snap.a * 10) {
                torn_read.store(true, std::memory_order_relaxed);
            }
        }
    });

    writer.join();
    reader.join();

    REQUIRE(torn_read.load() == false);
    REQUIRE(data.a == iterations);
    REQUIRE(data.b == iterations * 10);
}
