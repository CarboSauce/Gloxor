#ifndef USE_MODULES
#include "glox/box.hpp"
#else
import std;
import glox;
#endif
#include "doctest.h"
#include "utils.hpp"

constexpr auto constexpr_test(int a)
{
    glox::box<int> val(a);
    *val += 10;
    return *val + 5;
}

static_assert(constexpr_test(5) == 20);

TEST_CASE("Box alloc")
{
    alloc_tracker::reset_counters();
    glox::box<alloc_tracker> a { 1 };

    REQUIRE(alloc_tracker::ctor_counter == 1);
    REQUIRE(alloc_tracker::move_ctor_counter == 0);
}

TEST_CASE("Static factory alloc box")
{
    alloc_tracker::reset_counters();
    auto b = glox::box<alloc_tracker>::create(1);

    REQUIRE(alloc_tracker::ctor_counter == 1);
    REQUIRE(alloc_tracker::move_ctor_counter == 0);

    auto p = *b.val();
    REQUIRE(p.value == 1);
}
