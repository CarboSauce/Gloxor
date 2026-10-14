#ifndef USE_MODULES
#include "glox/format.hpp"
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
