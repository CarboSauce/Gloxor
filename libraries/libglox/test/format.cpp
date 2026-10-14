#ifndef USE_MODULES
#include "glox/format.hpp"
#include <format>
#include <string>
#include <string_view>
#else
import std;
import glox;
#endif
#include "doctest.h"

struct string_buffer
{
    std::string buf;
    size_t write(char c)
    {
        buf.push_back(c);
        return 1;
    }
    size_t flush()
    {
        buf.clear();
        return buf.length();
    }
};

TEST_CASE("Basic hello world fmt")
{
    string_buffer str;
    auto res = glox::format_to(str, "Hello {}", "World");
    REQUIRE(str.buf == "Hello World");
}

TEST_CASE("Basic hello world fmt string view")
{
    using namespace std::literals;
    string_buffer str;
    auto res = glox::format_to(str, "Hello {}", "World"sv);
    REQUIRE(str.buf == "Hello World");
}

TEST_CASE("Basic integer formatting")
{
    using namespace std::literals;
    string_buffer str;
    auto res = glox::format_to(str, "{} {}", -1, 1);
    REQUIRE(str.buf == "-1 1");
    str.flush();
    res = glox::format_to(str, "{} {}", -1212, 1212);
    REQUIRE(str.buf == "-1212 1212");
    auto min = std::numeric_limits<int>::min();
    auto max = std::numeric_limits<int>::max();
    str.flush();
    res = glox::format_to(str, "{} {}", min, max);
    REQUIRE(str.buf == std::format("{} {}", min, max));
    auto minl = std::numeric_limits<int64_t>::min();
    auto maxl = std::numeric_limits<uint64_t>::max();
    str.flush();
    res = glox::format_to(str, "{} {}", minl, maxl);
    REQUIRE(str.buf == std::format("{} {}", minl, maxl));
}
