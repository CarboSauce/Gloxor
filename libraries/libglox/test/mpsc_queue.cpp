
#ifndef USE_MODULES
#include "glox/mpsc_queue.hpp"

#include <atomic>
#include <cstddef>
#include <thread>
#include <vector>
#else
import std;
import glox;
#endif

#include "doctest.h"
#include "glox/macros.hpp"

struct test_item
{
    std::size_t value = 0;
    glox::mpsc_node mpsc_node;
};

TEST_CASE("intrusive_mpsc_queue single-threaded operations")
{
    glox::intrusive_mpsc_queue<test_item> queue;

    test_item a { 1 };
    test_item b { 2 };
    test_item c { 3 };

    REQUIRE(queue.empty());
    REQUIRE(queue.pop() == nullptr);

    queue.push(&a);
    queue.push(&b);
    queue.push(&c);

    REQUIRE_FALSE(queue.empty());

    REQUIRE(queue.pop() == &a);
    REQUIRE(queue.pop() == &b);
    REQUIRE(queue.pop() == &c);

    REQUIRE(queue.pop() == nullptr);
    REQUIRE(queue.empty());
}

TEST_CASE("intrusive_mpsc_queue preserves FIFO ordering")
{
    constexpr std::size_t total_items = 1000;

    glox::intrusive_mpsc_queue<test_item> queue;
    std::vector<test_item> items(total_items);

    for (std::size_t i = 0; i < total_items; ++i) {
        items[i].value = i;
        queue.push(&items[i]);
    }

    for (std::size_t i = 0; i < total_items; ++i) {
        auto* item = queue.pop();

        REQUIRE(item != nullptr);
        REQUIRE(item == &items[i]);
        REQUIRE(item->value == i);
    }

    REQUIRE(queue.pop() == nullptr);
    REQUIRE(queue.empty());
}

TEST_CASE("intrusive_mpsc_queue concurrent producers and single consumer")
{
    constexpr std::size_t producer_count = 4;
    constexpr std::size_t items_per_producer = 2500;
    constexpr std::size_t total_items = producer_count * items_per_producer;

    glox::intrusive_mpsc_queue<test_item> queue;

    std::vector<test_item> items(total_items);

    for (std::size_t i = 0; i < total_items; ++i)
        items[i].value = i;

    std::atomic<std::size_t> producers_done { 0 };

    std::vector<std::thread> producers;
    producers.reserve(producer_count);

    for (std::size_t p = 0; p < producer_count; ++p) {
        producers.emplace_back([&, p]() {
            const std::size_t begin = p * items_per_producer;
            const std::size_t end = begin + items_per_producer;

            for (std::size_t i = begin; i < end; ++i)
                queue.push(&items[i]);

            producers_done.fetch_add(1, std::memory_order_release);
        });
    }

    std::vector<bool> received(total_items, false);
    std::size_t received_count = 0;

    while (received_count < total_items) {
        test_item* item = queue.pop();

        if (!item) {
            std::this_thread::yield();
            continue;
        }

        REQUIRE(item->value < total_items);
        REQUIRE_FALSE(received[item->value]);

        received[item->value] = true;
        ++received_count;
    }

    for (auto& producer : producers)
        producer.join();

    REQUIRE(producers_done.load(std::memory_order_acquire) == producer_count);

    REQUIRE(received_count == total_items);

    for (std::size_t i = 0; i < total_items; ++i)
        REQUIRE(received[i]);

    REQUIRE(queue.pop() == nullptr);
    REQUIRE(queue.empty());
}

TEST_CASE("intrusive_mpsc_queue supports node reuse after pop")
{
    glox::intrusive_mpsc_queue<test_item> queue;

    test_item a { 1 };
    test_item b { 2 };

    queue.push(&a);
    REQUIRE(queue.pop() == &a);

    // The node can be reused after pop returns it.
    queue.push(&a);
    queue.push(&b);

    REQUIRE(queue.pop() == &a);
    REQUIRE(queue.pop() == &b);
    REQUIRE(queue.empty());
}
