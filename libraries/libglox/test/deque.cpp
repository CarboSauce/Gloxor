#ifndef USE_MODULES
#include "glox/deque.hpp"
#include <algorithm> // For std::sort, std::reverse
#include <cstdio>
#include <cstdlib>
#else
import std;
import glox;
#endif
#include "doctest.h"
#include "glox/macros.hpp"
#include "utils.hpp"

TEST_CASE("Deque default construction")
{
    glox::deque<alloc_tracker> d;
    REQUIRE(d.size() == 0);
    REQUIRE(d.empty() == true);
}

TEST_CASE("Deque push_back and indexing")
{
    glox::deque<int> d;
    d.push_back(1);
    d.push_back(2);
    d.push_back(3);
    d.push_back(4);

    REQUIRE(d.size() == 4);
    REQUIRE(d[0] == 1);
    REQUIRE(d[1] == 2);
    REQUIRE(d[2] == 3);
    REQUIRE(d[3] == 4);
    REQUIRE(d.front() == 1);
    REQUIRE(d.back() == 4);
}

TEST_CASE("Deque push_front and indexing")
{
    glox::deque<int> d;
    d.push_front(1);
    d.push_front(2);
    d.push_front(3);
    d.push_front(4);

    REQUIRE(d.size() == 4);
    // Pushing front reverses the order
    REQUIRE(d[0] == 4);
    REQUIRE(d[1] == 3);
    REQUIRE(d[2] == 2);
    REQUIRE(d[3] == 1);
    REQUIRE(d.front() == 4);
    REQUIRE(d.back() == 1);
}

TEST_CASE("Deque emplace_back lvalue performs copy")
{
    alloc_tracker::reset_counters();
    glox::deque<alloc_tracker> d;
    alloc_tracker x { 42 };

    d.emplace_back(x);

    REQUIRE(d.size() == 1);
    REQUIRE(d[0].value == 42);
    REQUIRE(alloc_tracker::copy_ctor_counter == 1);
    REQUIRE(alloc_tracker::move_ctor_counter == 0);
}

TEST_CASE("Deque emplace_back rvalue performs move")
{
    alloc_tracker::reset_counters();
    glox::deque<alloc_tracker> d;

    d.emplace_back(alloc_tracker { 42 });

    REQUIRE(d.size() == 1);
    REQUIRE(d[0].value == 42);
    REQUIRE(alloc_tracker::move_ctor_counter == 1);
    REQUIRE(alloc_tracker::copy_ctor_counter == 0);
}

TEST_CASE("Deque crosses block boundaries correctly (Large insertion)")
{
    alloc_tracker::reset_counters();
    glox::deque<alloc_tracker> d;

    // By default, MinElements is 16. Pushing 100 elements will force
    // the deque to allocate multiple blocks and possibly reallocate the map.
    for (int i = 0; i < 100; ++i) {
        d.emplace_back(i);
    }

    REQUIRE(d.size() == 100);
    REQUIRE(d.front().value == 0);
    REQUIRE(d.back().value == 99);
    REQUIRE(d[50].value == 50);

    // Unlike vector, deque doesn't move existing elements when growing the map
    // It only moves pointers to blocks.
    // Expect 100 ctor/moves, but no re-moves of existing elements.
    REQUIRE(
        alloc_tracker::move_ctor_counter == 0
    ); // Using emplace_back(int) directly constructs in place
    REQUIRE(alloc_tracker::copy_ctor_counter == 0);
}

TEST_CASE("Deque pop_back and pop_front")
{
    alloc_tracker::reset_counters();
    glox::deque<alloc_tracker> d;
    d.emplace_back(1);
    d.emplace_back(2);
    d.emplace_back(3);
    d.emplace_back(4);

    REQUIRE(d.size() == 4);

    d.pop_back();
    REQUIRE(d.size() == 3);
    REQUIRE(d.back().value == 3);
    REQUIRE(alloc_tracker::dtor_counter == 1);

    d.pop_front();
    REQUIRE(d.size() == 2);
    REQUIRE(d.front().value == 2);
    REQUIRE(alloc_tracker::dtor_counter == 2);
}

TEST_CASE("Deque copies data from other deque")
{
    glox::deque<alloc_tracker> d;
    d.emplace_back(1);
    d.emplace_back(2);
    d.emplace_back(3);

    alloc_tracker::reset_counters();

    auto d2 = d;
    REQUIRE(d.size() == 3);
    REQUIRE(d[0].value == 1);
    REQUIRE(d.back().value == 3);

    REQUIRE(d2.size() == 3);
    REQUIRE(d2[0].value == 1);
    REQUIRE(d2.back().value == 3);

    // Copying a deque copies the elements
    REQUIRE(alloc_tracker::copy_ctor_counter == 3);
    REQUIRE(alloc_tracker::move_ctor_counter == 0);
}

TEST_CASE("Deque moves data from other deque")
{
    glox::deque<alloc_tracker> d;
    d.emplace_back(1);
    d.emplace_back(2);
    d.emplace_back(3);

    alloc_tracker::reset_counters();

    auto d2 = std::move(d);
    REQUIRE(d.size() == 0); // Moved-from state

    REQUIRE(d2.size() == 3);
    REQUIRE(d2[0].value == 1);
    REQUIRE(d2.back().value == 3);

    // Moving a deque only swaps internal pointers, no element moves
    REQUIRE(alloc_tracker::move_ctor_counter == 0);
    REQUIRE(alloc_tracker::copy_ctor_counter == 0);
}

TEST_CASE("Deque clear destroys all elements")
{
    glox::deque<alloc_tracker> d;
    d.emplace_back(1);
    d.emplace_back(2);
    d.emplace_back(3);

    alloc_tracker::reset_counters();

    d.clear();
    REQUIRE(d.size() == 0);
    REQUIRE(d.empty() == true);
    REQUIRE(alloc_tracker::dtor_counter == 3);
}

TEST_CASE("Deque push_front and push_back interchangeably")
{
    glox::deque<int> d;
    d.push_back(1); // [1]
    d.push_front(2); // [2, 1]
    d.push_back(3); // [2, 1, 3]
    d.push_front(4); // [4, 2, 1, 3]

    REQUIRE(d.size() == 4);
    REQUIRE(d[0] == 4);
    REQUIRE(d[1] == 2);
    REQUIRE(d[2] == 1);
    REQUIRE(d[3] == 3);
}

TEST_CASE("Deque iterators range-based for loop")
{
    glox::deque<int> d;
    d.push_back(1);
    d.push_back(2);
    d.push_back(3);

    int sum = 0;
    for (int val : d) {
        sum += val;
    }
    REQUIRE(sum == 6);
}

TEST_CASE("Deque iterators random access arithmetic")
{
    glox::deque<int> d;
    for (int i = 0; i < 10; ++i) {
        d.push_back(i);
    }

    auto it = d.begin();
    REQUIRE(*it == 0);
    REQUIRE(*(it + 5) == 5);
    REQUIRE(*(5 + it) == 5);

    it += 5;
    REQUIRE(*it == 5);

    it -= 2;
    REQUIRE(*it == 3);

    REQUIRE(it[-1] == 2); // operator[]

    auto end_it = d.end();
    REQUIRE(end_it - d.begin() == 10); // difference operator
}

TEST_CASE("Deque iterator comparisons")
{
    glox::deque<int> d;
    d.push_back(10);
    d.push_back(20);
    d.push_back(30);

    auto it1 = d.begin();
    auto it2 = d.begin() + 1;
    auto it3 = d.begin() + 1;

    REQUIRE(it1 < it2);
    REQUIRE(it2 > it1);
    REQUIRE(it2 <= it3);
    REQUIRE(it2 >= it3);
    REQUIRE(it2 == it3);
    REQUIRE(it1 != it2);
}

TEST_CASE("Deque iterators work with std::sort and std::reverse")
{
    glox::deque<int> d;
    d.push_back(5);
    d.push_back(2);
    d.push_back(9);
    d.push_back(1);
    d.push_back(7);

    // Requires RandomAccessIterator
    std::sort(d.begin(), d.end());

    REQUIRE(d[0] == 1);
    REQUIRE(d[1] == 2);
    REQUIRE(d[2] == 5);
    REQUIRE(d[3] == 7);
    REQUIRE(d[4] == 9);

    // Requires BidirectionalIterator
    std::reverse(d.begin(), d.end());

    REQUIRE(d[0] == 9);
    REQUIRE(d[1] == 7);
    REQUIRE(d[2] == 5);
    REQUIRE(d[3] == 2);
    REQUIRE(d[4] == 1);
}

TEST_CASE("Deque const iterators")
{
    glox::deque<int> d;
    d.push_back(42);

    const auto& cd = d;
    auto it = cd.cbegin();

    REQUIRE(*it == 42);
    // *it = 10; // This should cause a compile error if uncommented

    // Implicit conversion from non-const to const iterator
    glox::deque<int>::const_iterator cit = d.begin();
    REQUIRE(cit == cd.begin());
}

TEST_CASE("Deque reserve expands map capacity")
{
    glox::deque<int> d;
    d.reserve(10000);

    // Doesn't change size, but ensures we don't reallocate the map when pushing
    REQUIRE(d.size() == 0);

    for (int i = 0; i < 10000; ++i) {
        d.push_back(i);
    }
    REQUIRE(d.size() == 10000);
    REQUIRE(d[5000] == 5000);
}

TEST_CASE("Deque insert in the middle (shifts right)")
{
    alloc_tracker::reset_counters();
    glox::deque<alloc_tracker> d;
    d.emplace_back(1);
    d.emplace_back(2);
    d.emplace_back(4);
    d.emplace_back(5);

    // Insert at offset 2 (closer to back, should shift suffix right)
    auto it = d.insert(d.begin() + 2, alloc_tracker { 3 });

    REQUIRE(d.size() == 5);
    REQUIRE(d[0].value == 1);
    REQUIRE(d[1].value == 2);
    REQUIRE(d[2].value == 3);
    REQUIRE(d[3].value == 4);
    REQUIRE(d[4].value == 5);
    REQUIRE((*it).value == 3);
}

TEST_CASE("Deque insert near front (shifts left)")
{
    alloc_tracker::reset_counters();
    glox::deque<alloc_tracker> d;
    d.emplace_back(1);
    d.emplace_back(3);
    d.emplace_back(4);
    d.emplace_back(5);
    d.emplace_back(6);

    // Insert at offset 1 (closer to front, should shift prefix left)
    auto it = d.insert(d.begin() + 1, alloc_tracker { 2 });

    REQUIRE(d.size() == 6);
    REQUIRE(d[0].value == 1);
    REQUIRE(d[1].value == 2);
    REQUIRE(d[2].value == 3);
    REQUIRE(d[3].value == 4);
}

TEST_CASE("Deque erase in the middle (shifts left)")
{
    alloc_tracker::reset_counters();
    glox::deque<alloc_tracker> d;
    d.emplace_back(1);
    d.emplace_back(2);
    d.emplace_back(99);
    d.emplace_back(3);
    d.emplace_back(4);

    // Erase at offset 2 (closer to back, should shift suffix left)
    auto it = d.erase(d.begin() + 2);

    REQUIRE(d.size() == 4);
    REQUIRE(d[0].value == 1);
    REQUIRE(d[1].value == 2);
    REQUIRE(d[2].value == 3);
    REQUIRE(d[3].value == 4);
    REQUIRE(
        (*it).value == 3
    ); // Iterator should point to the element following the erased one
}

TEST_CASE("Deque erase near front (shifts right)")
{
    alloc_tracker::reset_counters();
    glox::deque<alloc_tracker> d;
    d.emplace_back(1);
    d.emplace_back(99);
    d.emplace_back(2);
    d.emplace_back(3);
    d.emplace_back(4);
    d.emplace_back(5);

    // Erase at offset 1 (closer to front, should shift prefix right)
    auto it = d.erase(d.begin() + 1);

    REQUIRE(d.size() == 5);
    REQUIRE(d[0].value == 1);
    REQUIRE(d[1].value == 2);
    REQUIRE(d[2].value == 3);
    REQUIRE((*it).value == 2);
}
