
#pragma once

#include "glox/intrusive.hpp"
#include "glox/macros.hpp"

#ifndef USE_MODULES
#include <atomic>
#endif

namespace glox {

GLOX_BEGIN_EXPORT

struct mpsc_node
{
    mpsc_node* next = nullptr;

    mpsc_node() = default;

    mpsc_node(mpsc_node const&) = delete;
    mpsc_node& operator=(mpsc_node const&) = delete;
};

template <typename T, mpsc_node T::* NodePtr = &T::mpsc_node>
class intrusive_mpsc_queue
{
    std::atomic<mpsc_node*> head { nullptr };

    // Single-consumer state.
    mpsc_node* consumer_head = nullptr;

    static T* ptr_from_node(mpsc_node* node)
    {
        return container_of(node, NodePtr);
    }

public:
    intrusive_mpsc_queue() = default;

    intrusive_mpsc_queue(intrusive_mpsc_queue const&) = delete;
    intrusive_mpsc_queue& operator=(intrusive_mpsc_queue const&) = delete;

    void push(T* item)
    {
        mpsc_node* node = &(item->*NodePtr);

        mpsc_node* old = head.load(std::memory_order_relaxed);

        do {
            node->next = old;
        } while (!head.compare_exchange_weak(
            old, node, std::memory_order_release, std::memory_order_relaxed
        ));
    }

    T* pop()
    {
        if (!consumer_head) {
            mpsc_node* current
                = head.exchange(nullptr, std::memory_order_acquire);

            while (current) {
                mpsc_node* next = current->next;

                current->next = consumer_head;
                consumer_head = current;

                current = next;
            }
        }

        if (!consumer_head)
            return nullptr;

        mpsc_node* result = consumer_head;

        consumer_head = result->next;
        result->next = nullptr;

        return ptr_from_node(result);
    }

    bool empty() const
    {
        return head.load(std::memory_order_relaxed) == nullptr
            && consumer_head == nullptr;
    }
};

GLOX_END_EXPORT

} // namespace glox
