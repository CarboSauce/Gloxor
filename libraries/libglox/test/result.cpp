#include "glox/result.hpp"
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
        return div_result::from_err(div_error::divide_by_zero);
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
    compare({ div_error::divide_by_zero }, { div_error::divide_by_zero })
    == true
);
