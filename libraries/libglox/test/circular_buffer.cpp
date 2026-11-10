#ifndef USE_MODULES
#include "glox/circular_buffer.hpp"
#include <atomic>
#include <thread>
#include <vector>
#else
import std;
import glox;
#endif
#include "doctest.h"
#include "glox/macros.hpp"

namespace glox {

TEST_CASE("circular_buffer push, pop, overflow")
{
    circular_buffer<int, 3> cb;

    REQUIRE(cb.empty());
    REQUIRE(cb.capacity() == 3);

    REQUIRE(cb.push(10));
    REQUIRE(cb.push(20));
    REQUIRE(cb.push(30));
    REQUIRE(cb.full());
    REQUIRE_FALSE(cb.push(40)); // Buffer is full

    REQUIRE(cb.front() == 10);
    REQUIRE(cb.back() == 30);

    auto val = cb.pop();
    REQUIRE(val.has_val());
    REQUIRE(val.val() == 10);
    REQUIRE(cb.size() == 2);

    REQUIRE(cb.push(40)); // Wrap around insert
    REQUIRE(cb.full());

    REQUIRE(cb.pop().val() == 20);
    REQUIRE(cb.pop().val() == 30);
    REQUIRE(cb.pop().val() == 40);
    REQUIRE(cb.empty());
}

TEST_CASE("atomic_circular_buffer single-threaded operations")
{
    atomic_circular_buffer<int, 3> atomic;

    REQUIRE(atomic.empty());
    REQUIRE(atomic.push(1));
    REQUIRE(atomic.push(2));
    REQUIRE(atomic.push(3));
    REQUIRE(atomic.full());
    REQUIRE_FALSE(atomic.push(4));

    REQUIRE(atomic.pop().val() == 1);
    REQUIRE(atomic.push(4)); // Wrap-around
    REQUIRE(atomic.pop().val() == 2);
    REQUIRE(atomic.pop().val() == 3);
    REQUIRE(atomic.pop().val() == 4);
    REQUIRE(atomic.empty());
}

TEST_CASE("atomic_circular_buffer concurrent producer consumer test")
{
    constexpr std::size_t total_items = 10'00;
    atomic_circular_buffer<std::size_t, 128> atomic;

    std::thread producer([&]() {
        for (std::size_t i = 0; i < total_items; ++i) {
            while (!atomic.push(i)) {
                std::this_thread::yield();
            }
        }
    });

    std::vector<std::size_t> received;
    received.reserve(total_items);

    std::thread consumer([&]() {
        while (received.size() < total_items) {
            auto val = atomic.pop();
            if (val.has_val()) {
                received.push_back(val.val());
            } else {
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();

    REQUIRE(received.size() == total_items);
    for (std::size_t i = 0; i < total_items; ++i) {
        REQUIRE(received[i] == i);
    }
}

} // namespace glox
