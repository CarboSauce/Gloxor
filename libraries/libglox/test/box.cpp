#ifndef USE_MODULES
#include "glox/box.hpp"
#else
import std;
import glox;
#endif
#include "doctest.h"
#include "utils.hpp"

// ============================================================================
// Constexpr Tests
// ============================================================================
constexpr auto constexpr_test(int a)
{
    glox::box<int> val(a);
    *val += 10;
    return *val + 5;
}

static_assert(constexpr_test(5) == 20);

constexpr auto constexpr_array_box_test()
{
    glox::box<int[]> arr(4, 10);
    arr[1] = 20;
    return arr.front() + arr[1] + arr.back();
}

static_assert(constexpr_array_box_test() == 40);

// ============================================================================
// box<T> Tests
// ============================================================================
TEST_CASE("Box default construction is null")
{
    glox::box<alloc_tracker> b;
    REQUIRE(b.is_null());
    REQUIRE_FALSE(static_cast<bool>(b));
    REQUIRE(b.get_ptr() == nullptr);
    REQUIRE_FALSE(b.get().has_val());
}

TEST_CASE("Box alloc and destruction")
{
    alloc_tracker::reset_counters();
    {
        glox::box<alloc_tracker> a { 1 };
        REQUIRE_FALSE(a.is_null());
        REQUIRE(static_cast<bool>(a));
        REQUIRE(a->value == 1);
        REQUIRE((*a).value == 1);
        REQUIRE(alloc_tracker::ctor_counter == 1);
        REQUIRE(alloc_tracker::move_ctor_counter == 0);
        REQUIRE(alloc_tracker::dtor_counter == 0);
    }
    REQUIRE(alloc_tracker::dtor_counter == 1);
}

TEST_CASE("Static factory alloc box")
{
    alloc_tracker::reset_counters();
    auto b = glox::box<alloc_tracker>::create(1);

    REQUIRE(b.has_val());
    REQUIRE(alloc_tracker::ctor_counter == 1);
    REQUIRE(alloc_tracker::move_ctor_counter == 0);

    auto& p = *b.val();
    REQUIRE(p.value == 1);
}

TEST_CASE("Box move construction and move assignment")
{
    alloc_tracker::reset_counters();
    {
        glox::box<alloc_tracker> a { 42 };
        glox::box<alloc_tracker> b = RVALUE(a);

        REQUIRE(a.is_null());
        REQUIRE_FALSE(b.is_null());
        REQUIRE(b->value == 42);
        REQUIRE(alloc_tracker::ctor_counter == 1);
        REQUIRE(alloc_tracker::move_ctor_counter == 0);
        REQUIRE(alloc_tracker::dtor_counter == 0);

        glox::box<alloc_tracker> c { 99 };
        c = RVALUE(b);
        REQUIRE(c->value == 42);
        REQUIRE(b->value == 99); // swap behavior in move assignment
    }
    REQUIRE(alloc_tracker::dtor_counter == 2);
}

TEST_CASE("Box copy construction and copy assignment")
{
    alloc_tracker::reset_counters();
    {
        glox::box<alloc_tracker> a { 7 };
        glox::box<alloc_tracker> b = a;

        REQUIRE_FALSE(a.is_null());
        REQUIRE_FALSE(b.is_null());
        REQUIRE(a.get_ptr() != b.get_ptr());
        REQUIRE(b->value == 7);
        REQUIRE(alloc_tracker::copy_ctor_counter == 1);

        glox::box<alloc_tracker> c { 10 };
        c = a;
        REQUIRE(c->value == 7);
        REQUIRE(alloc_tracker::copy_ctor_counter == 2);
    }
    REQUIRE(alloc_tracker::dtor_counter == 4);
}

// ============================================================================
// box<T[]> Tests
// ============================================================================
TEST_CASE("Box array default and count construction")
{
    alloc_tracker::reset_counters();
    {
        glox::box<alloc_tracker[]> empty;
        REQUIRE(empty.is_null());
        REQUIRE(empty.size() == 0);

        glox::box<alloc_tracker[]> arr(3);
        REQUIRE_FALSE(arr.is_null());
        REQUIRE(arr.size() == 3);
        REQUIRE(alloc_tracker::ctor_counter == 3);
        REQUIRE(alloc_tracker::dtor_counter == 0);
    }
    REQUIRE(alloc_tracker::dtor_counter == 3);
}

TEST_CASE("Box array fill construction, indexing, and iterators")
{
    alloc_tracker::reset_counters();
    {
        alloc_tracker proto { 5 };
        alloc_tracker::reset_counters();

        glox::box<alloc_tracker[]> arr(4, proto);
        REQUIRE(arr.size() == 4);
        REQUIRE(alloc_tracker::copy_ctor_counter == 4);

        arr[0].value = 10;
        arr[3].value = 40;
        REQUIRE(arr.front().value == 10);
        REQUIRE(arr.back().value == 40);

        int sum = 0;
        for (const auto& item : arr) {
            sum += item.value;
        }
        REQUIRE(sum == 10 + 5 + 5 + 40);
    }
    REQUIRE(alloc_tracker::dtor_counter == 5);
}

TEST_CASE("Box array copy and move semantics")
{
    alloc_tracker::reset_counters();
    {
        auto opt_arr = glox::box<alloc_tracker[]>::create(2);
        REQUIRE(opt_arr.has_val());

        auto arr1 = RVALUE(opt_arr).val();
        arr1[0].value = 11;
        arr1[1].value = 22;

        alloc_tracker::reset_counters();
        auto arr2 = arr1; // deep copy
        REQUIRE(arr2.size() == 2);
        REQUIRE(arr2[0].value == 11);
        REQUIRE(arr2[1].value == 22);
        REQUIRE(alloc_tracker::copy_ctor_counter == 2);

        auto arr3 = RVALUE(arr1); // move
        REQUIRE(arr1.is_null());
        REQUIRE(arr1.size() == 0);
        REQUIRE(arr3.size() == 2);
        REQUIRE(arr3[1].value == 22);
    }
    REQUIRE(alloc_tracker::dtor_counter == 4);
}

// ============================================================================
// polybox<T> Tests
// ============================================================================
namespace {

struct poly_base
{
    // Intentionally non-virtual destructor to verify polybox tracks derived
    // dtor
    int base_val;
    explicit poly_base(int v)
        : base_val(v)
    {
    }
    virtual int compute() const
    {
        return base_val;
    }
};

struct poly_derived : poly_base
{
    alloc_tracker tracker;

    poly_derived(int base_v, int tracked_v)
        : poly_base(base_v)
        , tracker(tracked_v)
    {
    }

    int compute() const override
    {
        return base_val + tracker.value;
    }
};

struct poly_move_only : poly_base
{
    alloc_tracker tracker;

    poly_move_only(int base_v, int tracked_v)
        : poly_base(base_v)
        , tracker(tracked_v)
    {
    }

    poly_move_only(const poly_move_only&) = delete;
    poly_move_only& operator=(const poly_move_only&) = delete;
    poly_move_only(poly_move_only&&) = default;
    poly_move_only& operator=(poly_move_only&&) = default;

    int compute() const override
    {
        return base_val * tracker.value;
    }
};

} // namespace

TEST_CASE("Polybox derived construction and non-virtual base destruction")
{
    alloc_tracker::reset_counters();
    {
        auto opt_pb = glox::polybox<poly_base>::make<poly_derived>(10, 25);
        REQUIRE(opt_pb.has_val());

        auto pb = RVALUE(opt_pb).val();
        REQUIRE_FALSE(pb.is_null());
        REQUIRE(pb->compute() == 35);
        REQUIRE(alloc_tracker::ctor_counter == 1);
        REQUIRE(alloc_tracker::dtor_counter == 0);
    }
    // Verifies poly_derived::~poly_derived() ran despite non-virtual
    // ~poly_base()
    REQUIRE(alloc_tracker::dtor_counter == 1);
}

TEST_CASE("Polybox polymorphic deep copy without slicing")
{
    alloc_tracker::reset_counters();
    {
        glox::polybox<poly_base> original = poly_derived { 100, 50 };
        REQUIRE(original.is_copyable());
        REQUIRE(original->compute() == 150);

        alloc_tracker::reset_counters();
        glox::polybox<poly_base> copied = original;

        REQUIRE_FALSE(copied.is_null());
        REQUIRE(copied.get_ptr() != original.get_ptr());
        REQUIRE(copied->compute() == 150);
        REQUIRE(alloc_tracker::copy_ctor_counter == 1);

        // Copy assignment
        glox::polybox<poly_base> assigned = poly_derived { 1, 2 };
        assigned = original;
        REQUIRE(assigned->compute() == 150);
        REQUIRE(alloc_tracker::copy_ctor_counter == 2);
    }
    REQUIRE(alloc_tracker::dtor_counter == 5);
}

TEST_CASE("Polybox move-only derived type and converting move")
{
    alloc_tracker::reset_counters();
    {
        glox::polybox<poly_move_only> derivedBox(glox::in_place, 6, 7);
        REQUIRE_FALSE(derivedBox.is_copyable());

        // Converting move: polybox<poly_move_only> -> polybox<poly_base>
        glox::polybox<poly_base> baseBox = RVALUE(derivedBox);
        REQUIRE(derivedBox.is_null());
        REQUIRE_FALSE(baseBox.is_null());
        REQUIRE_FALSE(baseBox.is_copyable());
        REQUIRE(baseBox->compute() == 42);
    }
    REQUIRE(alloc_tracker::dtor_counter == 1);
}
