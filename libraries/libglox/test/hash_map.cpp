#ifndef USE_MODULES
#include "glox/hash_map.hpp"
#else
import std;
import glox;
#endif
#include "doctest.h"
#include "utils.hpp"

namespace {
// Forces every key into the same bucket to exercise chain handling.
struct const_hash
{
    size_t operator()(int) const
    {
        return 42;
    }
};
} // namespace

TEST_CASE("hash_map: empty map")
{
    glox::hash_map<int, int> m;

    REQUIRE(m.empty());
    REQUIRE(m.size() == 0);
    REQUIRE(not m.find(1).has_val());
    REQUIRE_FALSE(m.contains(1));
    REQUIRE_FALSE(m.erase(1));
}

TEST_CASE("hash_map: insert and find")
{
    glox::hash_map<int, int> m;
    auto val = m.insert_or_assign(1, 10);
    REQUIRE(val);
    REQUIRE(m.insert_or_assign(2, 20));

    REQUIRE(m.size() == 2);
    REQUIRE(m.contains(1));
    REQUIRE(*m.find(1) == 10);
    REQUIRE(*m.find(2) == 20);
    REQUIRE(not m.find(3).has_val());
}

TEST_CASE("hash_map: insert_or_assign overwrites")
{
    glox::hash_map<int, int> m;

    REQUIRE(m.insert_or_assign(1, 10));
    REQUIRE_FALSE(m.insert_or_assign(1, 99)); // existing key

    REQUIRE(m.size() == 1);
    REQUIRE(*m.find(1) == 99);
}

TEST_CASE("hash_map: operator[] default-inserts")
{
    glox::hash_map<int, int> m;

    REQUIRE(m[5] == 0);
    REQUIRE(m.size() == 1);

    m[5] = 7;
    REQUIRE(m.size() == 1);
    REQUIRE(*m.find(5) == 7);
}

TEST_CASE("hash_map: at returns option")
{
    glox::hash_map<int, int> m;
    m.insert_or_assign(1, 10);

    auto hit = m.at(1);
    REQUIRE(hit.has_val());
    REQUIRE(hit.val() == 10);

    auto miss = m.at(2);
    REQUIRE_FALSE(miss.has_val());
    REQUIRE(m.size() == 1); // at() never inserts

    const auto& cm = m;
    REQUIRE(cm.at(1).has_val());
    REQUIRE_FALSE(cm.at(2).has_val());
}

TEST_CASE("hash_map: erase")
{
    glox::hash_map<int, int> m;
    m.insert_or_assign(1, 10);
    m.insert_or_assign(2, 20);

    REQUIRE(m.erase(1));
    REQUIRE_FALSE(m.erase(1)); // already gone
    REQUIRE(m.size() == 1);
    REQUIRE_FALSE(m.contains(1));
    REQUIRE(m.contains(2));
}

TEST_CASE("hash_map: colliding keys share a chain")
{
    glox::hash_map<int, int, const_hash> m;
    for (int i = 0; i < 5; ++i)
        m.insert_or_assign(i, i * 10);

    REQUIRE(m.size() == 5);
    REQUIRE(m.find(0).val() == 0);
    REQUIRE(m.find(1).val() == 10);
    REQUIRE(m.find(2).val() == 20);
    REQUIRE(m.find(3).val() == 30);
    REQUIRE(m.find(4).val() == 40);

    // Insertion is push_front, so 4 is the head and 0 is the tail.
    REQUIRE(m.erase(4)); // head
    REQUIRE(m.erase(2)); // middle
    REQUIRE(m.erase(0)); // tail

    REQUIRE(m.size() == 2);
    REQUIRE(m.contains(1));
    REQUIRE(m.contains(3));
    REQUIRE_FALSE(m.contains(0));
    REQUIRE_FALSE(m.contains(2));
    REQUIRE_FALSE(m.contains(4));
}

TEST_CASE("hash_map: growth keeps all entries")
{
    glox::hash_map<int, int> m;
    constexpr int n = 1000;

    for (int i = 0; i < n; ++i)
        m.insert_or_assign(i, i * 2);

    REQUIRE(m.size() == n);
    REQUIRE(m.bucket_count() >= static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        auto v = m.find(i);
        REQUIRE(v.has_val());
        REQUIRE(*v == i * 2);
    }
}

TEST_CASE("hash_map: pointers stay valid across rehash")
{
    glox::hash_map<int, int> m;
    m.insert_or_assign(0, 123);
    auto p = m.find(0);

    for (int i = 1; i < 500; ++i)
        m.insert_or_assign(i, i);

    REQUIRE(
        m.find(0).operator->() == p.operator->()
    ); // nodes are relinked, never reallocated
    REQUIRE(*p == 123);
}

TEST_CASE("hash_map: clear")
{
    glox::hash_map<int, int> m;
    for (int i = 0; i < 50; ++i)
        m.insert_or_assign(i, i);

    m.clear();

    REQUIRE(m.empty());
    REQUIRE_FALSE(m.contains(10));

    m.insert_or_assign(1, 1); // still usable after clear
    REQUIRE(m.size() == 1);
}

TEST_CASE("hash_map: move construct")
{
    glox::hash_map<int, int> a;
    a.insert_or_assign(1, 10);
    a.insert_or_assign(2, 20);

    glox::hash_map<int, int> b(static_cast<glox::hash_map<int, int>&&>(a));

    REQUIRE(b.size() == 2);
    REQUIRE(*b.find(1) == 10);
    REQUIRE(a.size() == 0);
}

TEST_CASE("hash_map: move assign")
{
    glox::hash_map<int, int> a;
    a.insert_or_assign(1, 10);

    glox::hash_map<int, int> b;
    b.insert_or_assign(7, 70);
    b.insert_or_assign(8, 80);

    b = static_cast<glox::hash_map<int, int>&&>(a);

    REQUIRE(b.size() == 1);
    REQUIRE(*b.find(1) == 10);
    REQUIRE_FALSE(b.contains(7)); // old contents freed
    REQUIRE(a.size() == 0);
}

TEST_CASE("hash_map: non-trivial value type")
{
    glox::hash_map<int, alloc_tracker> m;
    alloc_tracker::reset_counters();

    m.insert_or_assign(1, alloc_tracker { 1 });
    REQUIRE(m.find(1)->value == 1);
}

TEST_CASE("hash_map: rehash does not move values")
{
    glox::hash_map<int, alloc_tracker> m;

    // 16 buckets initially with max load factor 1.0, so these don't rehash.
    for (int i = 0; i < 16; ++i)
        m.insert_or_assign(i, alloc_tracker { i });

    alloc_tracker::reset_counters();

    // The 17th insert triggers a rehash. Only the new value is moved.
    m.insert_or_assign(16, alloc_tracker { 16 });

    REQUIRE(alloc_tracker::ctor_counter == 1);
    REQUIRE(alloc_tracker::move_ctor_counter == 1);
    for (int i = 0; i <= 16; ++i)
        REQUIRE(m.find(i)->value == i);
}
