#ifndef USE_MODULES
#include "glox/string.hpp"
#include <cstdio>
#include <cstdlib>
#else
import std;
import glox;
import std.compat;
#endif
#include "doctest.h"
#include "glox/macros.hpp"
#include "utils.hpp"

constexpr auto test(size_t n)
{
    glox::string t = glox::string { "Hello" };

    for (size_t i = 0; i != n; ++i) {
        t.push_back('a');
    }

    return t.size();
}

constexpr size_t test_cap(size_t n)
{
    glox::string t = glox::string { n };

    return t.capacity();
}

using namespace std::string_view_literals;
constexpr bool test(std::string_view str, std::string_view comp)
{
    auto t = glox::string(str);

    t.append("Test");

    return t == comp;
}

constexpr bool test_popback(std::string_view str)
{
    auto t = glox::string(str);

    t.pop_back();

    return t == str.subview(0, str.size() - 1);
}

constexpr bool test_insert(std::string_view str, std::string_view comp)
{
    auto t = glox::string(str);

    t.insert(t.begin(), 'A');

    return t == comp;
}

constexpr bool test_erase(std::string_view str, std::string_view comp)
{
    auto t = glox::string(str);

    t.erase(t.begin());

    return t == comp;
}
static_assert(test(17) == 22);
static_assert(test(18) == 23);
static_assert(test(20) == 25);
static_assert(test_cap(23) == 23);
static_assert(test_cap(24) == 32);
static_assert(test("Hi", "HiTest"));
static_assert(test("VeryLongStringTest", "VeryLongStringTestTest"));
static_assert(test_popback("Test"));
static_assert(test_popback("VeryLongStringTestTest"));
static_assert(test_insert("Test", "ATest"));
static_assert(test_insert("VeryLongStringTestTest", "AVeryLongStringTestTest"));
static_assert(test_erase("AVeryLongStringTestTest", "VeryLongStringTestTest"));
static_assert(test_erase("AVeryLongStringTestTest", "VeryLongStringTestTest"));
