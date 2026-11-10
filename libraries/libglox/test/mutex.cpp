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

static_assert(mutex_type<spinlock>, "spinlock must satisfy glox::mutex_type");
static_assert(
    mutex_type<fair_spinlock>,
    "fair_spinlock must satisfy glox::mutex_type"
);

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

    // Main thread takes lock first to hold the line
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

    // Wait for all threads to be ready
    while (ready_count.load() < num_threads) {
        std::this_thread::yield();
    }

    // Release main lock to release queued threads in ticket order
    start_signal.store(true);
    lock.unlock();

    for (auto& t : threads) {
        t.join();
    }

    REQUIRE(execution_order.size() == static_cast<std::size_t>(num_threads));
}
