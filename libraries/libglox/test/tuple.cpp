#ifndef USE_MODULES
#include "glox/tuple.hpp"
#else
import std;
import glox;
#endif
#include "doctest.h"

static_assert(
    std::is_trivially_default_constructible_v<glox::tuple<int, int, int>>
    and std::is_trivially_copyable_v<glox::tuple<int, int, int>> == true
);

constexpr auto test_tuple()
{
    auto tpl = glox::tuple { 1, 2, 3 };
    glox::apply([]([[maybe_unused]] auto... args) { }, tpl);
    glox::for_each(tpl, []([[maybe_unused]] auto arg) { });
    auto [a, b, c] = tpl;
    return a + b + c;
}
static_assert(test_tuple() == 6, "Test constexpr tuple");

TEST_CASE("Tuple basic functions")
{
    auto tpl = glox::tuple { "Hello", "World", 5 };

    REQUIRE(glox::get<0>(tpl) == "Hello");
    REQUIRE(glox::get<1>(tpl) == "World");
    REQUIRE(glox::get<2>(tpl) == 5);
}

TEST_CASE("Tuple apply")
{
    auto tpl = glox::tuple { 1, 2, 3 };

    auto assertExpr = [](auto a, auto b) { REQUIRE(a == b); };

    glox::apply(
        [&, i = 0](const auto&... args) mutable {
            (assertExpr(args, ++i), ...);
        },
        tpl
    );
}

TEST_CASE("Tuple apply")
{
    auto tpl = glox::tuple { 1, 2, 3 };

    auto assertExpr = [](auto a, auto b) { REQUIRE(a == b); };

    glox::apply(
        [&, i = 0](const auto&... args) mutable {
            (assertExpr(args, ++i), ...);
        },
        tpl
    );
}

TEST_CASE("Tuple for each")
{
    auto tpl = glox::tuple { 1, 2, 3 };

    glox::for_each(tpl, [&, i = 0](auto&& val) mutable {
        REQUIRE(val == ++i);
    });
}
