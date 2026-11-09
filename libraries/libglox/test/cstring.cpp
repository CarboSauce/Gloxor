#ifndef USE_MODULES
#include "glox/string.hpp"
#include <string_view>
#else
import std;
import glox;
import std.compat;
#endif
#include "doctest.h"

using namespace std::string_view_literals;

// --- constexpr logic tests ---

constexpr bool test_cstring_basic()
{
    glox::cstring s("Hello");
    // Ensure size masks the \0, and equality works
    if (s.size() != 5 || s != "Hello"sv)
        return false;
    // Check that the null terminator is actually there at the exact end of the
    // view
    if (s.data()[s.size()] != '\0')
        return false;

    return true;
}

constexpr bool test_cstring_push_pop()
{
    glox::cstring s("Test");

    s.push_back('s'); // "Tests"
    if (s.size() != 5 || s.back() != 's')
        return false;
    if (s.data()[s.size()] != '\0')
        return false;

    s.pop_back(); // "Test"
    if (s.size() != 4 || s.back() != 't')
        return false;
    if (s.data()[s.size()] != '\0')
        return false;

    return true;
}

constexpr bool test_cstring_append()
{
    glox::cstring s("Hello");
    s.append(", World!");
    if (s.size() != 13 || s != "Hello, World!"sv)
        return false;
    if (s.data()[s.size()] != '\0')
        return false;

    s += " Test";
    if (s.size() != 18 || s != "Hello, World! Test"sv)
        return false;
    if (s.data()[s.size()] != '\0')
        return false;

    return true;
}

constexpr bool test_cstring_clear()
{
    glox::cstring s("Hello");
    s.clear();
    if (!s.empty() || s.size() != 0)
        return false;
    // Even when empty, a valid C-string must point to a \0
    if (s.data()[0] != '\0')
        return false;

    return true;
}

constexpr bool test_cstring_access()
{
    glox::cstring s("Access");
    if (s.front() != 'A')
        return false;
    if (s.back() != 's')
        return false; // Should not be \0
    if (s[1] != 'c')
        return false;

    // Iterator range should exactly match the string length without the \0
    size_t dist = s.end() - s.begin();
    if (dist != 6)
        return false;

    return true;
}

constexpr bool test_cstring_from_glox_string()
{
    glox::string str("Native");
    glox::cstring cstr(std::move(str));

    if (cstr.size() != 6 || cstr != "Native"sv)
        return false;
    if (cstr.data()[cstr.size()] != '\0')
        return false;

    return true;
}

// --- Static Asserts ---

static_assert(test_cstring_basic());
static_assert(test_cstring_push_pop());
static_assert(test_cstring_append());
static_assert(test_cstring_clear());
static_assert(test_cstring_access());
static_assert(test_cstring_from_glox_string());

// --- Runtime doctests for deep memory inspection ---

TEST_CASE("glox::cstring explicit memory verification")
{
    SUBCASE("Default constructor ensures \\0")
    {
        glox::cstring s;
        CHECK(s.empty() == true);
        CHECK(s.size() == 0);
        CHECK(s.c_str()[0] == '\0');
    }

    SUBCASE("String_view constructor ensures \\0")
    {
        std::string_view view = "kernel";
        glox::cstring s(view);
        CHECK(s.size() == 6);
        CHECK(s == "kernel");
        CHECK(s.c_str()[6] == '\0'); // Verify raw memory
    }

    SUBCASE("Mutations safely shift the \\0")
    {
        glox::cstring s("abc");
        CHECK(s.c_str()[3] == '\0');

        s.push_back('d');
        CHECK(s.size() == 4);
        CHECK(s.c_str()[4] == '\0'); // \0 moved right

        s.pop_back();
        CHECK(s.size() == 3);
        CHECK(s.c_str()[3] == '\0'); // \0 moved left

        s.append("defg");
        CHECK(s.size() == 7);
        CHECK(s.c_str()[7] == '\0'); // \0 survived a large append
    }

    SUBCASE("clear() preserves a safe C-string")
    {
        glox::cstring s("temporary");
        s.clear();
        CHECK(s.size() == 0);
        CHECK(s.c_str()[0] == '\0');

        // Pushing after clear should still maintain layout
        s.push_back('x');
        CHECK(s.size() == 1);
        CHECK(s.c_str()[1] == '\0');
    }

    SUBCASE("Iterators do not expose the \\0")
    {
        glox::cstring s("loop");
        size_t iterations = 0;
        for (char c : s) {
            CHECK(c != '\0'); // We should never loop over the null terminator
            iterations++;
        }
        CHECK(iterations == 4);
    }
}
