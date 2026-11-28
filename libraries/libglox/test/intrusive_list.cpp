#ifndef USE_MODULES
#include "glox/intrusive_list.hpp"
#include "glox/detail/moveutils.hpp"
#include <array>
#include <cstddef>
#include <iterator>
#else
import std;
import glox;
#endif
#include "doctest.h"
#include "glox/macros.hpp"

namespace {

struct test_struct
{
    int x;
    glox::list_node list_node;
    glox::list_node list2_node;
};

using test1list = glox::intrusive_list<test_struct>;
using test2list = glox::intrusive_list<test_struct, &test_struct::list2_node>;

struct test_struct_other_node
{
    int x;
    glox::list_node other_node = { };
};

using test_list_other_node = glox::intrusive_list<
    test_struct_other_node,
    &test_struct_other_node::other_node
>;

static_assert(std::bidirectional_iterator<test1list::iterator>);
static_assert(std::bidirectional_iterator<test1list::const_iterator>);

} // namespace

TEST_CASE("intrusive_list linking with custom member node pointer")
{
    test_list_other_node list;
    test_struct_other_node s[3] { { 1 }, { 2 }, { 3 } };

    REQUIRE(list.is_empty());
    REQUIRE(list.size() == 0);

    list.push_back(&s[0]);
    list.push_back(&s[1]);
    list.push_back(&s[2]);

    REQUIRE_FALSE(list.is_empty());
    REQUIRE(list.size() == 3);
    REQUIRE(list.front().x == 1);
    REQUIRE(list.back().x == 3);

    int expected = 1;
    for (auto const& item : list) {
        REQUIRE(item.x == expected++);
    }
    REQUIRE(expected == 4);
}

TEST_CASE("intrusive_list dual hooks and independent lifetimes")
{
    test1list l1 { };
    test2list l2 { };
    test_struct s[6] { { 1 }, { 2 }, { 3 }, { 4 }, { 5 }, { 6 } };

    REQUIRE(l1.is_empty());
    REQUIRE(l2.is_empty());

    for (int i = 0; i < 6; ++i) {
        l1.push_back(&s[i]);
        l2.push_back(&s[i]);
    }

    int i = 1;
    for (auto const& it : l1) {
        REQUIRE(it.x == i++);
    }

    l1.clear();
    REQUIRE(l1.is_empty());
    REQUIRE(l1.size() == 0);
    REQUIRE_FALSE(l2.is_empty());

    REQUIRE(l2.size() == 6);
    REQUIRE(l2.front().x == s[0].x);
    REQUIRE(l2.back().x == s[5].x);
    REQUIRE(l2.begin()->x == s[0].x);
    REQUIRE((--l2.end())->x == s[5].x);
    REQUIRE(l2.begin().next()->x == s[1].x);
}

TEST_CASE("intrusive_list push_front, insert, and erase")
{
    test1list list;
    test_struct s[5] { { 10 }, { 20 }, { 30 }, { 40 }, { 50 } };

    // push_front: [30, 20, 10]
    list.push_front(&s[0]);
    list.push_front(&s[1]);
    list.push_front(&s[2]);
    REQUIRE(list.size() == 3);
    REQUIRE(list.front().x == 30);
    REQUIRE(list.back().x == 10);

    // insert before middle (20): [30, 40, 20, 10]
    auto mid = list.begin().next();
    auto inserted = list.insert(mid, &s[3]);
    REQUIRE(inserted->x == 40);
    REQUIRE(list.size() == 4);

    // insert at end(): [30, 40, 20, 10, 50]
    list.insert(list.end(), &s[4]);
    REQUIRE(list.size() == 5);
    REQUIRE(list.back().x == 50);

    // erase middle (20): [30, 40, 10, 50]
    auto after_erased = list.erase(test1list::iterator(&s[1]));
    REQUIRE(after_erased->x == 10);
    REQUIRE(list.size() == 4);

    // Verify unlinked node points to itself
    REQUIRE(s[1].list_node.next == &s[1].list_node);
    REQUIRE(s[1].list_node.prev == &s[1].list_node);

    // erase front (30) and back (50): [40, 10]
    auto new_front = list.erase(list.begin());
    REQUIRE(new_front->x == 40);
    auto after_back = list.erase(--list.end());
    REQUIRE(after_back == list.end());
    REQUIRE(list.size() == 2);
    REQUIRE(list.front().x == 40);
    REQUIRE(list.back().x == 10);
}

TEST_CASE("intrusive_list clear with callback visits every node")
{
    test1list list;
    test_struct s[4] { { 1 }, { 2 }, { 3 }, { 4 } };

    for (auto& item : s) {
        list.push_back(&item);
    }

    int visited_count = 0;
    int sum = 0;
    list.clear([&](test_struct* node) {
        ++visited_count;
        sum += node->x;
        // Node must already be unlinked when callback runs
        REQUIRE(node->list_node.next == &node->list_node);
        REQUIRE(node->list_node.prev == &node->list_node);
    });

    REQUIRE(list.is_empty());
    REQUIRE(list.size() == 0);
    REQUIRE(visited_count == 4);
    REQUIRE(sum == 10);
}

TEST_CASE("intrusive_list move construction and move assignment")
{
    test_struct s[4] { { 10 }, { 20 }, { 30 }, { 40 } };

    // 1. Move non-empty list
    test1list src;
    src.push_back(&s[0]);
    src.push_back(&s[1]);
    src.push_back(&s[2]);

    test1list dst = RVALUE(src);
    REQUIRE(src.is_empty());
    REQUIRE(src.size() == 0);
    REQUIRE(src.begin() == src.end());

    REQUIRE(dst.size() == 3);
    REQUIRE(dst.front().x == 10);
    REQUIRE(dst.back().x == 30);

    // Mutate moved-to list to verify sentinel pointers were properly re-linked
    dst.push_back(&s[3]);
    REQUIRE(dst.size() == 4);
    REQUIRE(dst.back().x == 40);

    int expected = 10;
    for (auto const& item : dst) {
        REQUIRE(item.x == expected);
        expected += 10;
    }
    REQUIRE(expected == 50);

    // 2. Move empty list into non-empty list via move assignment
    test1list empty_src;
    dst = RVALUE(empty_src);
    REQUIRE(dst.is_empty());
    REQUIRE(dst.size() == 0);
    REQUIRE(dst.begin() == dst.end());
}

TEST_CASE("intrusive_list const iteration and reverse traversal")
{
    test1list list;
    test_struct s[3] { { 100 }, { 200 }, { 300 } };
    for (auto& item : s) {
        list.push_back(&item);
    }

    test1list const& clist = list;
    REQUIRE(clist.size() == 3);
    REQUIRE(clist.front().x == 100);
    REQUIRE(clist.back().x == 300);

    // Forward const iteration
    std::array<int, 3> forward_vals { };
    std::size_t idx = 0;
    for (auto it = clist.begin(); it != clist.end(); ++it) {
        forward_vals[idx++] = it->x;
    }
    REQUIRE(forward_vals[0] == 100);
    REQUIRE(forward_vals[1] == 200);
    REQUIRE(forward_vals[2] == 300);

    // Backward const iteration
    std::array<int, 3> reverse_vals { };
    idx = 0;
    for (auto it = clist.end(); it != clist.begin();) {
        --it;
        reverse_vals[idx++] = (*it).x;
    }
    REQUIRE(reverse_vals[0] == 300);
    REQUIRE(reverse_vals[1] == 200);
    REQUIRE(reverse_vals[2] == 100);
}
