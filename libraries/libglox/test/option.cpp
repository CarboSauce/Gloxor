#include "glox/option.hpp"
#include <string>
#include <type_traits>

static_assert(std::is_trivially_destructible_v<glox::option<int>>);
static_assert(std::is_trivially_copyable_v<glox::option<int>>);
static_assert(std::is_trivially_copy_constructible_v<glox::option<int>>);
static_assert(std::is_trivially_move_constructible_v<glox::option<int>>);
static_assert(std::is_trivially_copy_assignable_v<glox::option<int>>);
static_assert(std::is_trivially_move_assignable_v<glox::option<int>>);

static_assert(not std::is_trivially_destructible_v<glox::option<std::string>>);
static_assert(not std::is_trivially_copyable_v<glox::option<std::string>>);
static_assert(
    not std::is_trivially_copy_constructible_v<glox::option<std::string>>
);
static_assert(
    not std::is_trivially_move_constructible_v<glox::option<std::string>>
);
static_assert(
    not std::is_trivially_copy_assignable_v<glox::option<std::string>>
);
static_assert(
    not std::is_trivially_move_assignable_v<glox::option<std::string>>
);

using div_result = glox::option<int>;

constexpr div_result divide(int a, int b)
{
    if (b == 0)
        return { };
    else
        return a / b;
}

static_assert(divide(10, 0).has_val() == false);
static_assert(divide(10, 1).has_val() == true);
static_assert(divide(10, 1).val() == 10);

constexpr auto compare(glox::option<int> a, glox::option<int> b)
{
    return a == b;
}
static_assert(compare({ 1 }, { 1 }));
static_assert(compare({ }, { }));
static_assert(compare({ 0 }, { 1 }) == false);

constexpr div_result divide_by_zero_or_else_zero(int a, int b)
{
    return divide(a, b).or_else([]() { return div_result(0); });
}
static_assert(divide_by_zero_or_else_zero(10, 0).val() == 0);

constexpr div_result divide_by_zero_transform(int a, int b)
{
    return divide(a, b).transform([]([[maybe_unused]] auto a) { return 0; });
}

static_assert(divide_by_zero_transform(10, 0).has_val() == false);
static_assert(divide_by_zero_transform(10, 1).val() == 0);

constexpr div_result divide_by_zero_and_then(int a, int b)
{
    return divide(a, b).and_then([]([[maybe_unused]] auto a) {
        return div_result(0);
    });
}
static_assert(divide_by_zero_and_then(10, 0).has_val() == false);
static_assert(divide_by_zero_and_then(10, 1).val() == 0);

constexpr auto test_option_ref()
{
    int a = 5;
    auto b = glox::option<int&>::from_val(a);
    *b = 10;
    return b.val();
}

constexpr auto test_option_transform_mutation()
{
    int a = 5;
    auto b = glox::option<int&>::from_val(a);
    return b.transform([](int& c) { return c = 10; });
}

constexpr auto test_option_const_ref()
{
    int a = 5;
    auto b = glox::option<const int&>::from_val(a);
    return b.val() + 5;
}

static_assert(test_option_ref() == 10);
static_assert(test_option_transform_mutation().val() == 10);
static_assert(test_option_const_ref() == 10);
