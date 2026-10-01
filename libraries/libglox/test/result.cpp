#include "glox/result.hpp"
#include "doctest.h"
#include "utils.hpp"
#include <expected>
#include <string>
#include <type_traits>

static_assert(std::is_trivially_destructible_v<glox::result<int, int>>);
static_assert(std::is_trivially_copyable_v<glox::result<int, int>>);
static_assert(std::is_trivially_copy_constructible_v<glox::result<int, int>>);
static_assert(std::is_trivially_move_constructible_v<glox::result<int, int>>);
static_assert(std::is_trivially_copy_assignable_v<glox::result<int, int>>);
static_assert(std::is_trivially_move_assignable_v<glox::result<int, int>>);

static_assert(
    not std::is_trivially_destructible_v<glox::result<std::string, int>>
);
static_assert(not std::is_trivially_copyable_v<glox::result<std::string, int>>);
static_assert(
    not std::is_trivially_copy_constructible_v<glox::result<std::string, int>>
);
static_assert(
    not std::is_trivially_move_constructible_v<glox::result<std::string, int>>
);
static_assert(
    not std::is_trivially_copy_assignable_v<glox::result<std::string, int>>
);
static_assert(
    not std::is_trivially_move_assignable_v<glox::result<std::string, int>>
);

static_assert(
    not std::is_trivially_destructible_v<glox::result<std::string, std::string>>
);
static_assert(
    not std::is_trivially_copyable_v<glox::result<std::string, std::string>>
);
static_assert(not std::is_trivially_copy_constructible_v<
    glox::result<std::string, std::string>
>);
static_assert(not std::is_trivially_move_constructible_v<
    glox::result<std::string, std::string>
>);
static_assert(not std::
        is_trivially_copy_assignable_v<glox::result<std::string, std::string>>);
static_assert(not std::
        is_trivially_move_assignable_v<glox::result<std::string, std::string>>);

static_assert(
    not std::is_trivially_destructible_v<glox::result<int, std::string>>
);
static_assert(
    not std::is_trivially_destructible_v<glox::result<std::string, int>>
);
static_assert(
    not std::is_trivially_destructible_v<glox::result<std::string, std::string>>
);

enum class div_error
{
    divide_by_zero
};

using div_result = glox::result<int, div_error>;

constexpr div_result divide(int a, int b)
{
    if (b == 0)
        return div_result(glox::error_inplace, div_error::divide_by_zero);
    else
        return a / b;
}

static_assert(divide(10, 0).has_val() == false);
static_assert(divide(10, 1).has_val() == true);
static_assert(divide(10, 1).val() == 10);
static_assert(divide(10, 0).err() == div_error::divide_by_zero);

constexpr div_result divide_by_zero_or_else_zero(int a, int b)
{
    return divide(a, b).or_else([]([[maybe_unused]] auto a) {
        return div_result(0);
    });
}
static_assert(divide_by_zero_or_else_zero(10, 0).val() == 0);

constexpr div_result divide_by_zero_transform(int a, int b)
{
    return divide(a, b).transform([]([[maybe_unused]] auto a) { return 0; });
}

static_assert(
    divide_by_zero_transform(10, 0).err() == div_error::divide_by_zero
);
static_assert(divide_by_zero_transform(10, 1).val() == 0);

constexpr div_result divide_by_zero_and_then(int a, int b)
{
    return divide(a, b).and_then([]([[maybe_unused]] auto a) {
        return div_result(0);
    });
}
static_assert(
    divide_by_zero_and_then(10, 0).err() == div_error::divide_by_zero
);
static_assert(divide_by_zero_and_then(10, 1).val() == 0);

constexpr auto divide_by_zero_transform_err(int a, int b)
{
    return divide(a, b).transform_err([]([[maybe_unused]] auto a) {
        return 0;
    });
}
static_assert(divide_by_zero_transform_err(10, 0).err() == 0);
static_assert(divide_by_zero_and_then(10, 1).val() == 0);

constexpr auto compare(div_result a, div_result b)
{
    return a == b;
}

static_assert(compare({ 1 }, { 1 }));
static_assert(compare({ 0 }, { 1 }) == false);
static_assert(
    compare(
        glox::error { div_error::divide_by_zero },
        glox::error { div_error::divide_by_zero }
    )
    == true
);

constexpr div_result try_test()
{
    auto res1 = divide(10, 1);
    auto res2 = divide(10, 2);
    return div_result(TRY(res1) + TRY(res2));
}
static_assert(try_test().val() == 15);

static_assert(not std::is_trivially_copy_assignable_v<std::string>);
static_assert(std::is_trivially_copy_assignable_v<int>);

constexpr glox::result<int, int> test_assignments()
{
    glox::result<std::string, int> a = std::string { "Test" };
    glox::result<std::string, int> b = std::string { "Test" };
    b = a;
    return glox::result<int, int>(
        (int)a.val().length() + (int)a.val().length()
    );
}

static_assert(test_assignments().val() == 8);

constexpr glox::result<alloc_tracker, div_error> div_test_alloc(int a, int b)
{
    if (b == 0) {
        return glox::error { div_error::divide_by_zero };
    } else {
        return glox::result<alloc_tracker, div_error> {
            { a / b },
        };
    }
}
auto test_func(int a, int b) -> glox::result<alloc_tracker, div_error>
{
    auto res = div_test_alloc(a, b);
    alloc_tracker::reset_counters();
    auto tmp = TRY(res);
    REQUIRE(alloc_tracker::move_ctor_counter == 1);
    REQUIRE(alloc_tracker::copy_ctor_counter == 0);
    REQUIRE(alloc_tracker::move_assignment_counter == 0);
    REQUIRE(alloc_tracker::copy_assignment_counter == 0);
    alloc_tracker::reset_counters();
    auto tmp2 = decltype(res)(RVALUE(tmp));
    alloc_tracker::reset_counters();
    return tmp2;
};

TEST_CASE("Test TRY macro for expected move count on value")
{
    alloc_tracker::reset_counters();
    std::expected<alloc_tracker, int> a { 1 };
    auto tmp = test_func(10, 1);
    REQUIRE(alloc_tracker::copy_ctor_counter == 0);
    REQUIRE(alloc_tracker::move_ctor_counter == 0);
    REQUIRE(alloc_tracker::move_assignment_counter == 0);
    REQUIRE(alloc_tracker::move_assignment_counter == 0);
}

TEST_CASE("Test TRY macro for expected move count on error")
{
    auto divtest = []([[maybe_unused]] int a,
                       int b) -> glox::result<int, alloc_tracker> {
        if (b == 0) {
            return glox::result<int, alloc_tracker>(glox::error_inplace, 1);
        } else
            return glox::result<int, alloc_tracker>(glox::in_place, a / b);
    };
    auto testFunc = [=](int a, int b) -> glox::result<int, alloc_tracker> {
        auto res = divtest(a, b);
        alloc_tracker::reset_counters();
        return decltype(res)(TRY(res) + 1);
    };

    auto res = testFunc(10, 0);

    REQUIRE(res.err().value == 1);
    REQUIRE(alloc_tracker::copy_ctor_counter == 0);
    REQUIRE(alloc_tracker::move_ctor_counter == 2);
    REQUIRE(alloc_tracker::move_assignment_counter == 0);
    REQUIRE(alloc_tracker::move_assignment_counter == 0);
}
