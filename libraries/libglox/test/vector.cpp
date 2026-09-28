#include "glox/vector.hpp"
#include "doctest.h"
#include "utils.hpp"
static_assert(
    sizeof(glox::vector<alloc_tracker>)
    == sizeof(alloc_tracker*) + sizeof(size_t) * 2
);
TEST_CASE("Vector default construction")
{
    glox::vector<alloc_tracker> v;
    REQUIRE(v.size() == 0);
    REQUIRE(v.capacity() == 0);
}

TEST_CASE("Vector indexing works")
{
    glox::vector<int> v;
    v.emplace_back(1);
    v.emplace_back(2);
    v.emplace_back(3);
    v.emplace_back(4);
    REQUIRE(v[0] == 1);
    REQUIRE(v[1] == 2);
    REQUIRE(v[2] == 3);
    REQUIRE(v[3] == 4);
}

TEST_CASE("Vector emplace_back lvalue performs copy")
{
    alloc_tracker::reset_counters();

    glox::vector<alloc_tracker> v;
    alloc_tracker x { 42 };

    v.emplace_back(x);

    REQUIRE(v.size() == 1);
    REQUIRE(v[0].value == 42);
    REQUIRE(alloc_tracker::copy_ctor_counter == 1);
    REQUIRE(alloc_tracker::move_ctor_counter == 0);
}

TEST_CASE("Vector emplace_back rvalue performs move")
{
    alloc_tracker::reset_counters();

    glox::vector<alloc_tracker> v;

    v.emplace_back(alloc_tracker { 42 });

    REQUIRE(v.size() == 1);
    REQUIRE(v.capacity() == 4);
    REQUIRE(v[0].value == 42);
    REQUIRE(alloc_tracker::move_ctor_counter == 1);
    REQUIRE(alloc_tracker::copy_ctor_counter == 0);
}

TEST_CASE("Vector push 2 elements reserves space for 4")
{
    alloc_tracker::reset_counters();

    glox::vector<alloc_tracker> v;
    v.emplace_back(alloc_tracker { 1 });
    v.emplace_back(alloc_tracker { 2 });

    REQUIRE(v.size() == 2);
    REQUIRE(v.capacity() == 4);
    REQUIRE(v[0].value == 1);
    REQUIRE(v[1].value == 2);

    REQUIRE(alloc_tracker::move_ctor_counter == 2);
    REQUIRE(alloc_tracker::copy_ctor_counter == 0);
    REQUIRE(alloc_tracker::move_assignment_counter == 0);
    REQUIRE(alloc_tracker::copy_assignment_counter == 0);
}

TEST_CASE("Vector reallocation on new element moves only old elements")
{

    glox::vector<alloc_tracker> v;

    v.emplace_back(alloc_tracker { 1 });
    v.emplace_back(alloc_tracker { 2 });
    v.emplace_back(alloc_tracker { 3 });
    v.emplace_back(alloc_tracker { 4 });

    alloc_tracker::reset_counters();
    v.emplace_back(5);

    REQUIRE(v.size() == 5);
    REQUIRE(v.capacity() >= 4);
    REQUIRE(v[0].value == 1);
    REQUIRE(v[1].value == 2);

    REQUIRE(alloc_tracker::move_ctor_counter == 4);
    REQUIRE(alloc_tracker::copy_ctor_counter == 0);
    REQUIRE(alloc_tracker::move_assignment_counter == 0);
    REQUIRE(alloc_tracker::copy_assignment_counter == 0);
}

TEST_CASE("Vector pop after push is size 0 and capacity non 0")
{
    glox::vector<int> v;
    v.emplace_back(0);
    v.pop_back();

    REQUIRE(v.size() == 0);
    REQUIRE(v.capacity() >= 1);
}

TEST_CASE("Vector copies data from other vector")
{
    glox::vector<alloc_tracker> v;
    v.emplace_back(1);
    v.emplace_back(2);
    v.emplace_back(3);
    v.emplace_back(4);
    v.emplace_back(5);
    alloc_tracker::reset_counters();

    auto v2 = v;
    REQUIRE(v.size() == 5);
    REQUIRE(v.capacity() >= 5);
    REQUIRE(v[0].value == 1);
    REQUIRE(v.back().value == 5);

    REQUIRE(v2.size() == 5);
    REQUIRE(v2.capacity() >= 5);
    REQUIRE(v2[0].value == 1);
    REQUIRE(v2.back().value == 5);
    REQUIRE(alloc_tracker::move_ctor_counter == 0);
    REQUIRE(alloc_tracker::copy_ctor_counter == 5);
}

TEST_CASE("Vector moves data from other vector")
{
    glox::vector<alloc_tracker> v;
    v.emplace_back(1);
    v.emplace_back(2);
    v.emplace_back(3);
    v.emplace_back(4);
    v.emplace_back(5);
    alloc_tracker::reset_counters();

    auto v2 = std::move(v);
    REQUIRE(v.size() == 0);
    REQUIRE(v.capacity() == 0);
    REQUIRE(v.is_null() == true);

    REQUIRE(v2.size() == 5);
    REQUIRE(v2.capacity() >= 5);
    REQUIRE(v2[0].value == 1);
    REQUIRE(v2.back().value == 5);
    REQUIRE(alloc_tracker::move_ctor_counter == 0);
    REQUIRE(alloc_tracker::copy_ctor_counter == 0);
}
