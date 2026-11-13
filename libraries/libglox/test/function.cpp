#ifndef USE_MODULES
#include "glox/function.hpp"
#include "glox/detail/moveutils.hpp"
#include <array>
#include <cstddef>
#else
import std;
import glox;
#endif
#include "doctest.h"
#include "utils.hpp"

namespace {

int free_add(int a, int b)
{
    return a + b;
}

void free_increment(int& x)
{
    ++x;
}

struct functor_mul
{
    int factor;
    int operator()(int x) const
    {
        return x * factor;
    }
};

struct stateful_counter
{
    int count = 0;
    int operator()()
    {
        return ++count;
    }
};

struct small_callable
{
    int a;
    int b;

    int operator()(int x) const
    {
        return (a + x) * b;
    }
};

struct tracked_callable
{
    alloc_tracker tracker;
    int multiplier;

    tracked_callable(int v, int m)
        : tracker(v)
        , multiplier(m)
    {
    }

    int operator()(int x) const
    {
        return (tracker.value + x) * multiplier;
    }
};

struct large_tracked_callable
{
    alloc_tracker tracker;
    std::array<std::size_t, 8> padding { 1, 2, 3, 4, 5, 6, 7, 8 };

    explicit large_tracked_callable(int v)
        : tracker(v)
    {
    }

    int operator()() const
    {
        return tracker.value + static_cast<int>(padding[0] + padding[7]);
    }
};

struct move_only_payload
{
    alloc_tracker tracker;

    explicit move_only_payload(int v)
        : tracker(v)
    {
    }

    move_only_payload(const move_only_payload&) = delete;
    move_only_payload& operator=(const move_only_payload&) = delete;
    move_only_payload(move_only_payload&&) = default;
    move_only_payload& operator=(move_only_payload&&) = default;
};

} // namespace

// ============================================================================
// glox::function_ref Tests
// ============================================================================
TEST_CASE("function_ref default and nullptr state")
{
    glox::function_ref<int(int, int)> empty;
    REQUIRE_FALSE(static_cast<bool>(empty));

    glox::function_ref<int(int, int)> null_ref = nullptr;
    REQUIRE_FALSE(static_cast<bool>(null_ref));

    int (*null_fn)(int, int) = nullptr;
    glox::function_ref<int(int, int)> from_null_ptr = null_fn;
    REQUIRE_FALSE(static_cast<bool>(from_null_ptr));
}

TEST_CASE("function_ref binds free functions and void return")
{
    glox::function_ref<int(int, int)> add_ref = &free_add;
    REQUIRE(static_cast<bool>(add_ref));
    REQUIRE(add_ref(3, 4) == 7);

    int value = 10;
    glox::function_ref<void(int&)> inc_ref = &free_increment;
    inc_ref(value);
    REQUIRE(value == 11);
}

TEST_CASE("function_ref binds lambdas, const functors, and mutating functors")
{
    int captured = 5;
    auto lambda = [&](int x) {
        captured += x;
        return captured;
    };

    glox::function_ref<int(int)> ref = lambda;
    REQUIRE(ref(3) == 8);
    REQUIRE(captured == 8);

    const functor_mul const_mul { 4 };
    glox::function_ref<int(int)> const_ref = const_mul;
    REQUIRE(const_ref(6) == 24);

    stateful_counter counter { 0 };
    glox::function_ref<int()> mut_ref = counter;
    REQUIRE(mut_ref() == 1);
    REQUIRE(mut_ref() == 2);
    REQUIRE(counter.count == 2);
}

TEST_CASE("function_ref never copies or moves tracked callables")
{
    alloc_tracker::reset_counters();
    {
        tracked_callable cb { 10, 2 };
        REQUIRE(alloc_tracker::ctor_counter == 1);

        glox::function_ref<int(int)> ref = cb;
        REQUIRE(alloc_tracker::copy_ctor_counter == 0);
        REQUIRE(alloc_tracker::move_ctor_counter == 0);
        REQUIRE(ref(5) == 30);
    }
    REQUIRE(alloc_tracker::dtor_counter == 1);
}

// ============================================================================
// glox::function<Sig, Allocator> Tests
// ============================================================================
TEST_CASE("function default, nullptr, and assignment reset")
{
    glox::function<int(int, int)> fn;
    REQUIRE_FALSE(static_cast<bool>(fn));
    REQUIRE_FALSE(fn.is_sbo());
    REQUIRE(fn.is_copyable());

    glox::function<int(int, int)> null_fn = nullptr;
    REQUIRE_FALSE(static_cast<bool>(null_fn));

    int (*raw_null)(int, int) = nullptr;
    glox::function<int(int, int)> from_null = raw_null;
    REQUIRE_FALSE(static_cast<bool>(from_null));

    fn = &free_add;
    REQUIRE(static_cast<bool>(fn));
    REQUIRE(fn.is_sbo());
    REQUIRE(fn(20, 22) == 42);

    fn = nullptr;
    REQUIRE_FALSE(static_cast<bool>(fn));
    REQUIRE_FALSE(fn.is_sbo());
}

TEST_CASE("function SBO storage for small callables")
{
    // Pure POD small_callable (8 bytes + 8-byte vptr = 16 bytes <= 24-byte SBO)
    glox::function<int(int)> pod_fn = small_callable { 4, 3 };
    REQUIRE(static_cast<bool>(pod_fn));
    REQUIRE(pod_fn.is_sbo());
    REQUIRE(pod_fn(6) == 30);

    // Tracked callable (if alloc_tracker fits within 16 bytes alongside vptr)
    alloc_tracker::reset_counters();
    {
        glox::function<int(int)> fn = tracked_callable { 4, 3 };
        REQUIRE(static_cast<bool>(fn));
        if constexpr (
            sizeof(tracked_callable) + sizeof(void*) <= sizeof(void*) * 3
            && alignof(tracked_callable) <= alignof(std::max_align_t)
        ) {
            REQUIRE(fn.is_sbo());
        }
        REQUIRE(fn(6) == 30);
        REQUIRE(alloc_tracker::dtor_counter == 1); // temporary destroyed
    }
    REQUIRE(alloc_tracker::dtor_counter == 2); // stored callable destroyed
}

TEST_CASE("function heap fallback for large callables")
{
    alloc_tracker::reset_counters();
    {
        glox::function<int()> fn = large_tracked_callable { 10 };
        REQUIRE(static_cast<bool>(fn));
        REQUIRE_FALSE(fn.is_sbo());
        REQUIRE(fn() == 19);
        REQUIRE(alloc_tracker::dtor_counter == 1); // temporary destroyed
    }
    REQUIRE(
        alloc_tracker::dtor_counter == 2
    ); // heap-allocated instance destroyed
}

TEST_CASE("function copy construction and copy assignment (SBO and heap)")
{
    // SBO deep copy
    glox::function<int(int)> sbo_a = small_callable { 5, 2 };
    glox::function<int(int)> sbo_b = sbo_a;
    REQUIRE(sbo_a.is_sbo());
    REQUIRE(sbo_b.is_sbo());
    REQUIRE(sbo_a(1) == 12);
    REQUIRE(sbo_b(1) == 12);

    // Tracked deep copy
    alloc_tracker::reset_counters();
    {
        glox::function<int(int)> tracked_a = tracked_callable { 5, 2 };
        glox::function<int(int)> tracked_b = tracked_a;
        REQUIRE(tracked_a(1) == 12);
        REQUIRE(tracked_b(1) == 12);
        REQUIRE(alloc_tracker::copy_ctor_counter >= 1);

        // Heap deep copy
        glox::function<int()> heap_a = large_tracked_callable { 100 };
        glox::function<int()> heap_b;
        heap_b = heap_a;
        REQUIRE_FALSE(heap_a.is_sbo());
        REQUIRE_FALSE(heap_b.is_sbo());
        REQUIRE(heap_a() == 109);
        REQUIRE(heap_b() == 109);
    }
    REQUIRE(
        alloc_tracker::dtor_counter == 6
    ); // 2 temporaries + 4 stored instances
}

TEST_CASE("function move construction and move assignment")
{
    glox::function<int(int)> sbo_a = small_callable { 3, 4 };
    glox::function<int(int)> sbo_b = RVALUE(sbo_a);
    REQUIRE_FALSE(static_cast<bool>(sbo_a));
    REQUIRE(sbo_b.is_sbo());
    REQUIRE(sbo_b(2) == 20);

    alloc_tracker::reset_counters();
    {
        glox::function<int(int)> tracked_a = tracked_callable { 3, 4 };
        glox::function<int(int)> tracked_b = RVALUE(tracked_a);
        REQUIRE_FALSE(static_cast<bool>(tracked_a));
        REQUIRE(tracked_b(2) == 20);

        glox::function<int()> heap_a = large_tracked_callable { 50 };
        glox::function<int()> heap_b;
        heap_b = RVALUE(heap_a);
        REQUIRE_FALSE(static_cast<bool>(heap_a));
        REQUIRE_FALSE(heap_b.is_sbo());
        REQUIRE(heap_b() == 59);
    }
    REQUIRE(
        alloc_tracker::dtor_counter == 5
    ); // 2 temporaries + 1 SBO move-out + 2 moved-to instances
}

TEST_CASE("function stateful mutable callable")
{
    glox::function<int()> counter = [n = 0]() mutable { return ++n; };

    REQUIRE(counter.is_sbo());
    REQUIRE(counter() == 1);
    REQUIRE(counter() == 2);

    auto copy = counter;
    REQUIRE(copy.is_sbo());
    REQUIRE(copy() == 3);
    REQUIRE(counter() == 3);
}

TEST_CASE("function supports move-only callables")
{
    alloc_tracker::reset_counters();
    {
        glox::function<int()> fn
            = [p = move_only_payload { 77 }]() { return p.tracker.value; };

        REQUIRE(static_cast<bool>(fn));
        REQUIRE_FALSE(fn.is_copyable());
        REQUIRE(fn() == 77);

        auto moved = RVALUE(fn);
        REQUIRE_FALSE(static_cast<bool>(fn));
        REQUIRE_FALSE(moved.is_copyable());
        REQUIRE(moved() == 77);
    }
    REQUIRE(
        alloc_tracker::dtor_counter == 3
    ); // 1 temporary move + 1 SBO move-out + 1 stored instance
}
