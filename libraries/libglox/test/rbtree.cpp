#ifndef USE_MODULES
#include "glox/intrusive_rb_tree.hpp"
#include <algorithm>
#include <cassert>
#include <iterator>
#include <map>
#include <random>
#include <string>
#include <vector>
#else
import std;
import glox;
#endif
#include "doctest.h"

using namespace glox;

namespace {
struct foo
{
    int key;
    rb_tree_node node;

    explicit foo(int k)
        : key { k }
        , node { }
    {
    }
};

struct foo_compare
{
    using is_transparent = void;

    bool operator()(const foo& a, const foo& b) const
    {
        return a.key < b.key;
    }
    bool operator()(const foo& a, int b) const
    {
        return a.key < b;
    }
    bool operator()(int a, const foo& b) const
    {
        return a < b.key;
    }
};

using foo_base_tree = intrusive_rb_tree<foo, &foo::node>;
using foo_tree = ordered_intrusive_rb_tree<foo, &foo::node, foo_compare>;

std::vector<foo> make_foos(int count)
{
    std::vector<foo> items;
    items.reserve(count);
    for (int i = 0; i < count; ++i) {
        items.emplace_back(i);
    }
    return items;
}

std::vector<int> shuffled_indices(int count, unsigned seed)
{
    std::vector<int> order(count);
    for (int i = 0; i < count; ++i) {
        order[i] = i;
    }
    std::shuffle(order.begin(), order.end(), std::mt19937 { seed });
    return order;
}
} // namespace

// Appends at the right end, which keeps order without any comparator.
foo_base_tree::iterator append(foo_base_tree& tree, foo& value)
{
    if (tree.empty()) {
        return tree.insert_at(value, tree.end_node(), tree.root_link());
    }
    rb_tree_node* last = &(*--tree.end()).node;
    return tree.insert_at(value, last, &last->right);
}

TEST_CASE("base tree: comparator-free structure")
{
    std::vector<foo> items = make_foos(500);
    foo_base_tree tree;
    CHECK(tree.empty());
    CHECK(tree.begin() == tree.end());
    CHECK(tree.verify());

    bool alwaysValid = true;
    for (foo& f : items) {
        append(tree, f);
        alwaysValid &= tree.verify();
    }
    CHECK(alwaysValid);
    CHECK_EQ(tree.size(), 500u);
    CHECK_EQ(tree.begin()->key, 0);
    CHECK_EQ((--tree.end())->key, 499);

    int expected = 0;
    bool inOrder = true;
    for (const foo& f : tree) {
        inOrder &= f.key == expected++;
    }
    CHECK(inOrder);

    bool eraseValid = true;
    for (int i : shuffled_indices(500, 3)) {
        tree.erase(items[i]);
        eraseValid &= tree.verify();
    }
    CHECK(eraseValid);
    CHECK(tree.empty());
}

TEST_CASE("ordered tree: empty")
{
    foo_tree tree;
    CHECK(tree.empty());
    CHECK_EQ(tree.size(), 0u);
    CHECK(tree.begin() == tree.end());
    CHECK(tree.rbegin() == tree.rend());
    CHECK(tree.find(1) == tree.end());
    CHECK(tree.lower_bound(1) == tree.end());
    CHECK(tree.upper_bound(1) == tree.end());
    CHECK(tree.verify());
}

TEST_CASE("ordered tree: insert keeps invariants and order")
{
    std::vector<foo> items = make_foos(1000);
    foo_tree tree;
    bool allInserted = true;
    bool alwaysValid = true;
    for (int i : shuffled_indices(1000, 1)) {
        allInserted &= tree.insert(items[i]).second;
        alwaysValid &= tree.verify();
    }
    CHECK(allInserted);
    CHECK(alwaysValid);
    CHECK_EQ(tree.size(), 1000u);

    int expected = 0;
    bool inOrder = true;
    for (const foo& f : tree) {
        inOrder &= f.key == expected++;
    }
    CHECK(inOrder);
    CHECK_EQ(expected, 1000);
}

TEST_CASE("ordered tree: duplicate insert is rejected")
{
    std::vector<foo> items = make_foos(10);
    foo duplicate { 5 };
    foo_tree tree;
    for (foo& f : items) {
        tree.insert(f);
    }
    auto result = tree.insert(duplicate);
    CHECK_FALSE(result.second);
    CHECK(&*result.first == &items[5]);
    CHECK_EQ(tree.size(), 10u);
    CHECK(tree.verify());
}

TEST_CASE("ordered tree: lookup")
{
    std::vector<foo> items = make_foos(100);
    foo_tree tree;
    for (foo& f : items) {
        tree.insert(f);
    }
    CHECK_EQ(tree.find(50)->key, 50);
    CHECK(tree.find(100) == tree.end());
    CHECK_EQ(tree.lower_bound(10)->key, 10);
    CHECK_EQ(tree.upper_bound(10)->key, 11);
    CHECK(tree.lower_bound(100) == tree.end());
    CHECK(tree.upper_bound(99) == tree.end());
    CHECK(tree.lower_bound(-5) == tree.begin());
}

TEST_CASE("ordered tree: erase keeps invariants")
{
    std::vector<foo> items = make_foos(1000);
    foo_tree tree;
    for (foo& f : items) {
        tree.insert(f);
    }
    bool alwaysValid = true;
    for (int i : shuffled_indices(1000, 2)) {
        tree.erase(items[i]);
        alwaysValid &= tree.verify();
    }
    CHECK(alwaysValid);
    CHECK(tree.empty());
    CHECK(tree.begin() == tree.end());
}

TEST_CASE("ordered tree: erase by iterator returns the next element")
{
    std::vector<foo> items = make_foos(10);
    foo_tree tree;
    for (foo& f : items) {
        tree.insert(f);
    }
    auto next = tree.erase(tree.find(3));
    CHECK_EQ(next->key, 4);
    next = tree.erase(tree.find(9));
    CHECK(next == tree.end());
    CHECK_EQ(tree.size(), 8u);
    CHECK(tree.verify());
}

TEST_CASE("ordered tree: --end() and reverse iteration")
{
    std::vector<foo> items = make_foos(100);
    foo_tree tree;
    for (foo& f : items) {
        tree.insert(f);
    }
    CHECK_EQ((--tree.end())->key, 99);
    CHECK_EQ(tree.rbegin()->key, 99);

    auto it = tree.end();
    --it;
    ++it;
    CHECK(it == tree.end());

    int expected = 99;
    bool inOrder = true;
    for (auto i = tree.end(); i != tree.begin();) {
        --i;
        inOrder &= i->key == expected--;
    }
    CHECK(inOrder);
    CHECK_EQ(expected, -1);
}

TEST_CASE("ordered tree: extremes are maintained across erase")
{
    std::vector<foo> items = make_foos(5);
    foo_tree tree;
    for (foo& f : items) {
        tree.insert(f);
    }
    tree.erase(items[4]);
    CHECK_EQ((--tree.end())->key, 3);
    tree.erase(items[0]);
    CHECK_EQ(tree.begin()->key, 1);
    tree.erase(items[1]);
    tree.erase(items[2]);
    CHECK_EQ(tree.begin()->key, 3);
    CHECK(std::prev(tree.end()) == tree.begin());
    tree.erase(items[3]);
    CHECK(tree.begin() == tree.end());
    CHECK(tree.verify());
}

TEST_CASE("ordered tree: move, swap and clear")
{
    std::vector<foo> items = make_foos(50);
    foo_tree a;
    for (foo& f : items) {
        a.insert(f);
    }

    foo_tree b { std::move(a) };
    CHECK(a.empty());
    CHECK(a.begin() == a.end());
    CHECK(a.verify());
    CHECK_EQ(b.size(), 50u);
    CHECK_EQ((--b.end())->key, 49);
    CHECK(b.verify());

    a = std::move(b);
    CHECK(b.empty());
    CHECK_EQ(a.size(), 50u);
    CHECK(a.verify());

    a.swap(b);
    CHECK(a.empty());
    CHECK_EQ(b.size(), 50u);
    CHECK(a.verify());
    CHECK(b.verify());

    b.clear();
    CHECK(b.empty());
    CHECK(b.verify());

    // elements can be reinserted after clear
    CHECK(a.insert(items[7]).second);
    CHECK(a.verify());
}

TEST_CASE("ordered tree: clear_and_dispose visits every element once")
{
    std::vector<foo> items = make_foos(200);
    foo_tree tree;
    for (foo& f : items) {
        tree.insert(f);
    }
    std::vector<int> seen;
    tree.clear([&](foo* f) { seen.push_back(f->key); });
    std::sort(seen.begin(), seen.end());
    CHECK_EQ(seen.size(), 200u);
    bool eachOnce = true;
    for (int i = 0; i < 200; ++i) {
        eachOnce &= seen[i] == i;
    }
    CHECK(eachOnce);
    CHECK(tree.empty());
    CHECK(tree.verify());
}

TEST_CASE("map: basic operations")
{
    rb_tree_map<int, std::string> map;
    CHECK(map.empty());
    CHECK(map.begin() == map.end());

    map.insert(2, "two");
    map.insert(1, "one");
    map.insert(3, "three");
    CHECK_EQ(map.size(), 3u);
    CHECK_EQ(map.at(1).val(), "one");
    CHECK(map.contains(2));
    CHECK_FALSE(map.contains(4));
    CHECK_EQ(map.count(3), 1u);
    CHECK_EQ(map.count(4), 0u);
    CHECK_EQ(map.begin()->first, 1);
    CHECK_EQ((--map.end())->first, 3);
    CHECK(map.verify());
}

TEST_CASE("map: at fails on missing key")
{
    rb_tree_map<int, int> map;
    map.insert(1, 1);
    CHECK(map.at(2).has_val() == false);
    const auto& constMap = map;
    CHECK(constMap.at(2).has_val() == false);
}

TEST_CASE("map: try_emplace, insert and insert_or_assign")
{
    rb_tree_map<std::string, int> map;

    auto first = map.try_emplace("a", 1);
    CHECK_EQ(first.second, glox::insert_result::success);
    auto second = map.try_emplace("a", 2);
    CHECK(second.second == glox::insert_result::key_exists);
    CHECK_EQ(map.at("a").val(), 1);

    CHECK(map.insert({ "b", 5 }).second == glox::insert_result::success);
    CHECK(map.insert({ "b", 6 }).second == glox::insert_result::key_exists);
    CHECK_EQ(map.at("b").val(), 5);

    CHECK(
        map.insert_or_assign("b", 7).second == glox::insert_result::key_exists
    );
    CHECK_EQ(map.at("b").val(), 7);
    CHECK(map.insert_or_assign("c", 8).second == glox::insert_result::success);
    CHECK_EQ(map.size(), 3u);
    CHECK(map.verify());
}

TEST_CASE("map: bounds")
{
    rb_tree_map<int, int> map;
    for (int i = 0; i < 100; i += 10) {
        map.insert(i, i);
    }
    CHECK_EQ(map.lower_bound(10)->first, 10);
    CHECK_EQ(map.lower_bound(11)->first, 20);
    CHECK_EQ(map.upper_bound(10)->first, 20);
    CHECK(map.lower_bound(91) == map.end());
    CHECK(map.upper_bound(90) == map.end());
}

TEST_CASE("map: custom comparator")
{
    rb_tree_map<int, int, std::greater<int>> map {
        { 1, 10 }, { 3, 30 }, { 2, 20 }
    };
    CHECK_EQ(map.begin()->first, 3);
    CHECK_EQ((--map.end())->first, 1);
    CHECK_EQ(map.at(2).val(), 20);
    CHECK_EQ(map.lower_bound(2)->first, 2);
    CHECK_EQ(map.upper_bound(2)->first, 1);
}

TEST_CASE("map: --end() and reverse iteration")
{
    rb_tree_map<int, int> map;
    map.insert(5, 50);
    CHECK(std::prev(map.end()) == map.begin());
    CHECK_EQ((--map.end())->first, 5);

    for (int i = 0; i < 100; ++i) {
        map.insert(i, i);
    }
    CHECK_EQ((--map.end())->first, 99);
    CHECK_EQ(map.rbegin()->first, 99);

    int expected = 99;
    bool inOrder = true;
    for (auto it = map.end(); it != map.begin();) {
        --it;
        inOrder &= it->first == expected--;
    }
    CHECK(inOrder);
    CHECK_EQ(expected, -1);
}

TEST_CASE("map: erase")
{
    rb_tree_map<int, int> map;
    for (int i = 0; i < 100; ++i) {
        map.insert(i, i);
    }
    CHECK_EQ(map.erase(99), 1u);
    CHECK_EQ(map.erase(99), 0u);
    CHECK_EQ((--map.end())->first, 98);
    CHECK_EQ(map.erase(0), 1u);
    CHECK_EQ(map.begin()->first, 1);
    CHECK(map.verify());

    for (auto it = map.begin(); it != map.end();) {
        it = (it->first % 2) ? map.erase(it) : std::next(it);
    }
    bool allEven = true;
    for (const auto& kv : map) {
        allEven &= kv.first % 2 == 0;
    }
    CHECK(allEven);
    CHECK(map.verify());
}

TEST_CASE("map: copy")
{
    rb_tree_map<int, std::string> map { { 1, "a" }, { 2, "b" }, { 3, "c" } };
    rb_tree_map<int, std::string> copy { map };
    CHECK_EQ(copy.size(), 3u);
    CHECK(copy.verify());
    copy.insert(1, "changed");
    CHECK_EQ(map.at(1).val(), "a");

    rb_tree_map<int, std::string> assigned;
    assigned.insert(9, "x");
    assigned = map;
    CHECK_EQ(assigned.size(), 3u);
    CHECK_FALSE(assigned.contains(9));
    CHECK(assigned.verify());
}

TEST_CASE("map: move, swap and clear")
{
    rb_tree_map<int, int> a;
    for (int i = 0; i < 100; ++i) {
        a.insert(i, i);
    }

    rb_tree_map<int, int> b { std::move(a) };
    CHECK(a.empty());
    CHECK(a.begin() == a.end());
    CHECK(a.verify());
    CHECK_EQ(b.size(), 100u);
    CHECK_EQ((--b.end())->first, 99);
    CHECK(b.verify());

    a = std::move(b);
    CHECK(b.empty());
    CHECK_EQ(a.size(), 100u);
    CHECK(a.verify());

    a.swap(b);
    CHECK(a.empty());
    CHECK_EQ(b.size(), 100u);

    b.clear();
    CHECK(b.begin() == b.end());
    CHECK(b.verify());
}

TEST_CASE("map: const iteration")
{
    const rb_tree_map<int, int> map { { 1, 1 }, { 2, 2 } };
    int sum = 0;
    for (const auto& kv : map) {
        sum += kv.second;
    }
    CHECK_EQ(sum, 3);
    CHECK(map.cbegin() == map.begin());
    CHECK_EQ((--map.cend())->first, 2);
}

TEST_CASE("map: randomized operations match std::map")
{
    rb_tree_map<int, std::string> map;
    std::map<int, std::string> reference;
    std::mt19937 rng { 42 };

    bool eraseMatches = true;
    bool boundsMatch = true;
    bool alwaysValid = true;
    for (int i = 0; i < 200000; ++i) {
        int key = rng() % 2000;
        switch (rng() % 4) {
        case 0:
        case 1:
            map.try_emplace(key, std::to_string(i));
            reference.try_emplace(key, std::to_string(i));
            break;
        case 2:
            eraseMatches &= map.erase(key) == reference.erase(key);
            break;
        case 3: {
            auto a = map.lower_bound(key);
            auto b = reference.lower_bound(key);
            boundsMatch &= (a == map.end()) == (b == reference.end());
            if (b != reference.end()) {
                boundsMatch &= a->first == b->first && a->second == b->second;
            }
            break;
        }
        }
        if (i % 1000 == 0) {
            alwaysValid &= map.verify();
        }
    }
    CHECK(eraseMatches);
    CHECK(boundsMatch);
    CHECK(alwaysValid);
    REQUIRE(map.size() == reference.size());

    auto it = map.begin();
    bool contentsMatch = true;
    for (const auto& [key, value] : reference) {
        contentsMatch &= it->first == key && it->second == value;
        ++it;
    }
    CHECK(contentsMatch);
    CHECK(it == map.end());
}
