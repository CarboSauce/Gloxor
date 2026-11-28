#ifndef USE_MODULES
#include "glox/list.hpp"
#include "glox/detail/moveutils.hpp"
#include <array>
#include <iterator>
#include <vector>
#else
import std;
import glox;
#endif
#include "doctest.h"
#include "glox/macros.hpp"
#include "utils.hpp"

namespace {

static_assert(std::bidirectional_iterator<glox::list<int>::iterator>);
static_assert(std::bidirectional_iterator<glox::list<int>::const_iterator>);

} // namespace

TEST_CASE("glox::list default, count, fill, and range construction")
{
    alloc_tracker::reset_counters();
    {
        glox::list<alloc_tracker> empty;
        REQUIRE(empty.empty());
        REQUIRE(empty.size() == 0);
        REQUIRE(empty.begin() == empty.end());

        glox::list<alloc_tracker> defaulted(3);
        REQUIRE_FALSE(defaulted.empty());
        REQUIRE(defaulted.size() == 3);
        REQUIRE(alloc_tracker::ctor_counter == 3);
        REQUIRE(alloc_tracker::dtor_counter == 0);

        alloc_tracker proto { 42 };
        alloc_tracker::reset_counters();
        glox::list<alloc_tracker> filled(4, proto);
        REQUIRE(filled.size() == 4);
        REQUIRE(alloc_tracker::copy_ctor_counter == 4);
        for (auto const& item : filled) {
            REQUIRE(item.value == 42);
        }

        std::array<int, 4> src { 10, 20, 30, 40 };
        glox::list<int> from_range(src.begin(), src.end());
        REQUIRE(from_range.size() == 4);
        REQUIRE(from_range.front() == 10);
        REQUIRE(from_range.back() == 40);
    }
    REQUIRE(alloc_tracker::dtor_counter == 8);
}

TEST_CASE("glox::list copy construction and copy assignment")
{
    alloc_tracker::reset_counters();
    {
        glox::list<alloc_tracker> a;
        a.emplace_back(1);
        a.emplace_back(2);
        a.emplace_back(3);

        alloc_tracker::reset_counters();
        glox::list<alloc_tracker> b = a;
        REQUIRE(b.size() == 3);
        REQUIRE(alloc_tracker::copy_ctor_counter == 3);

        int expected = 1;
        for (auto const& item : b) {
            REQUIRE(item.value == expected++);
        }

        // Mutating copy does not affect original
        b.front().value = 99;
        REQUIRE(a.front().value == 1);

        glox::list<alloc_tracker> c;
        c.emplace_back(100);
        alloc_tracker::reset_counters();
        c = a;
        REQUIRE(c.size() == 3);
        REQUIRE(alloc_tracker::dtor_counter == 1);
        REQUIRE(alloc_tracker::copy_ctor_counter == 3);
        REQUIRE(c.back().value == 3);
    }
    REQUIRE(alloc_tracker::dtor_counter == 10);
}

TEST_CASE("glox::list move construction and move assignment")
{
    alloc_tracker::reset_counters();
    {
        glox::list<alloc_tracker> a;
        a.emplace_back(10);
        a.emplace_back(20);
        a.emplace_back(30);

        glox::list<alloc_tracker> b = RVALUE(a);
        REQUIRE(a.empty());
        REQUIRE(a.size() == 0);
        REQUIRE(b.size() == 3);
        REQUIRE(b.front().value == 10);
        REQUIRE(b.back().value == 30);
        REQUIRE(alloc_tracker::move_ctor_counter == 0);
        REQUIRE(alloc_tracker::copy_ctor_counter == 0);

        // Verify moved-to list remains valid for subsequent insertions
        b.emplace_back(40);
        REQUIRE(b.size() == 4);
        REQUIRE(b.back().value == 40);

        glox::list<alloc_tracker> c;
        c.emplace_back(99);
        c = RVALUE(b);
        REQUIRE(b.empty());
        REQUIRE(c.size() == 4);
        REQUIRE(c.front().value == 10);
        REQUIRE(c.back().value == 40);
    }
    REQUIRE(alloc_tracker::dtor_counter == 5);
}

TEST_CASE("glox::list push, pop, insert, and erase")
{
    glox::list<int> l;
    l.push_back(20);
    l.push_front(10);
    l.push_back(40);
    // [10, 20, 40]
    REQUIRE(l.size() == 3);
    REQUIRE(l.front() == 10);
    REQUIRE(l.back() == 40);

    // Insert single element before 40 -> [10, 20, 30, 40]
    auto it_40 = --l.end();
    auto it_30 = l.insert(it_40, 30);
    REQUIRE(*it_30 == 30);
    REQUIRE(l.size() == 4);

    // Insert range before 20 -> [10, 15, 16, 20, 30, 40]
    std::array<int, 2> extra { 15, 16 };
    auto it_20 = ++l.begin();
    auto first_ins = l.insert(it_20, extra.begin(), extra.end());
    REQUIRE(*first_ins == 15);
    REQUIRE(l.size() == 6);

    // Erase single element (16) -> [10, 15, 20, 30, 40]
    auto after_16 = l.erase(++first_ins);
    REQUIRE(*after_16 == 20);
    REQUIRE(l.size() == 5);

    // Erase subrange [20, 40) -> [10, 15, 40]
    auto after_range = l.erase(after_16, --l.end());
    REQUIRE(*after_range == 40);
    REQUIRE(l.size() == 3);

    l.pop_front();
    l.pop_back();
    REQUIRE(l.size() == 1);
    REQUIRE(l.front() == 15);
    REQUIRE(l.back() == 15);
}

TEST_CASE("glox::list resize, assign, and swap")
{
    glox::list<int> l;
    l.assign(4, 7);
    REQUIRE(l.size() == 4);
    for (int v : l) {
        REQUIRE(v == 7);
    }

    std::array<int, 2> smaller { 1, 2 };
    l.assign(smaller.begin(), smaller.end());
    REQUIRE(l.size() == 2);
    REQUIRE(l.front() == 1);
    REQUIRE(l.back() == 2);

    l.resize(5, 9);
    REQUIRE(l.size() == 5);
    REQUIRE(l.back() == 9);

    l.resize(1);
    REQUIRE(l.size() == 1);
    REQUIRE(l.front() == 1);

    glox::list<int> other;
    other.push_back(100);
    other.push_back(200);

    l.swap(other);
    REQUIRE(l.size() == 2);
    REQUIRE(l.front() == 100);
    REQUIRE(other.size() == 1);
    REQUIRE(other.front() == 1);
}

TEST_CASE("glox::list splice single element and whole list")
{
    alloc_tracker::reset_counters();
    {
        glox::list<alloc_tracker> dst;
        dst.emplace_back(1);
        dst.emplace_back(4);

        glox::list<alloc_tracker> src;
        src.emplace_back(2);
        src.emplace_back(3);

        // Splice single element (2) before 4 -> dst: [1, 2, 4], src: [3]
        dst.splice(--dst.end(), src, src.begin());
        REQUIRE(dst.size() == 3);
        REQUIRE(src.size() == 1);
        REQUIRE(src.front().value == 3);

        // Splice remainder of src before 4 -> dst: [1, 2, 3, 4], src: []
        dst.splice(--dst.end(), src);
        REQUIRE(src.empty());
        REQUIRE(dst.size() == 4);

        // Zero node copies or moves during splice
        REQUIRE(alloc_tracker::copy_ctor_counter == 0);
        REQUIRE(alloc_tracker::move_ctor_counter == 0);

        int expected = 1;
        for (auto const& item : dst) {
            REQUIRE(item.value == expected++);
        }
    }
    REQUIRE(alloc_tracker::dtor_counter == 4);
}

TEST_CASE("glox::list remove, remove_if, unique, reverse, and operator==")
{
    glox::list<int> l;
    for (int v : { 1, 2, 2, 3, 2, 4, 4, 5 }) {
        l.push_back(v);
    }

    // unique removes consecutive duplicates -> [1, 2, 3, 2, 4, 5]
    REQUIRE(l.unique() == 2);
    REQUIRE(l.size() == 6);

    // remove all 2s -> [1, 3, 4, 5]
    REQUIRE(l.remove(2) == 2);
    REQUIRE(l.size() == 4);

    // remove_if odd -> [4]
    REQUIRE(l.remove_if([](int x) { return (x % 2) != 0; }) == 3);
    REQUIRE(l.size() == 1);
    REQUIRE(l.front() == 4);

    l.clear();
    for (int v : { 10, 20, 30, 40 }) {
        l.push_back(v);
    }
    l.reverse();

    glox::list<int> expected;
    for (int v : { 40, 30, 20, 10 }) {
        expected.push_back(v);
    }
    REQUIRE(l == expected);

    // Reverse iteration check
    std::vector<int> rev;
    for (auto it = l.crbegin(); it != l.crend(); ++it) {
        rev.push_back(*it);
    }
    REQUIRE(rev == std::vector<int> { 10, 20, 30, 40 });
}
