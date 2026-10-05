#include "doctest.h"
#include "glox/intrusive_list.hpp"
#include <cassert>
#include <cstdio>
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

TEST_CASE("Linking without list_node in struct")
{
    test_list_other_node list;
    test_struct_other_node s[6] { { 1 }, { 2 }, { 3 } };
    list.push_back(&s[0]);
    list.push_back(&s[1]);
    list.push_back(&s[2]);
}

TEST_CASE("General usecase")
{
    test1list l1 { };
    test2list l2 { };
    test_struct s[6] { { 1 }, { 2 }, { 3 }, { 4 }, { 5 }, { 6 } };
    REQUIRE(l1.is_empty());
    REQUIRE(l2.is_empty());
    for (int i = 0; i != 6; ++i) {
        l1.push_back(s + i);
    }
    for (int i = 0; i != 6; ++i)
        l2.push_back(s + i);

    int i = 1;
    for (const auto& it : l1) {
        REQUIRE(it.x == i++);
    }
    l1.clear();
    REQUIRE(l1.is_empty());
    REQUIRE(not l2.is_empty());

    REQUIRE(l2.size() == 6);
    REQUIRE(l2.front().x == s[0].x);
    REQUIRE(l2.back().x == s[5].x);
    REQUIRE(l2.begin()->x == s[0].x);
    REQUIRE((--l2.end())->x == s[5].x);
    REQUIRE(l2.begin().next()->x == s[1].x);
}
