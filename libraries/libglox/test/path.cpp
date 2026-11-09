#ifndef USE_MODULES
#include "glox/path.hpp"
#include "glox/string.hpp"
#include <cstdio>
#include <cstdlib>
#include <string_view>
#else
import std;
import glox;
import std.compat;
#endif
#include "doctest.h"
#include "glox/macros.hpp"
#include "utils.hpp"

using namespace std::string_view_literals;

// --- path_view constexpr tests ---

constexpr bool test_pv_filename(std::string_view str, std::string_view comp)
{
    return glox::path_view(str).filename() == glox::path_view(comp);
}

constexpr bool test_pv_parent(std::string_view str, std::string_view comp)
{
    return glox::path_view(str).parent_path() == glox::path_view(comp);
}

constexpr bool test_pv_extension(std::string_view str, std::string_view comp)
{
    return glox::path_view(str).extension() == glox::path_view(comp);
}

constexpr bool test_pv_stem(std::string_view str, std::string_view comp)
{
    return glox::path_view(str).stem() == glox::path_view(comp);
}

constexpr bool test_pv_is_absolute(std::string_view str, bool expected)
{
    return glox::path_view(str).is_absolute() == expected;
}

// --- path constexpr tests ---

constexpr bool test_path_append(
    std::string_view base,
    std::string_view next,
    std::string_view comp
)
{
    auto p = glox::path(glox::path_view(base));
    p /= glox::path_view(next);
    return p == glox::path_view(comp);
}

constexpr bool test_path_operator_slash(
    std::string_view base,
    std::string_view next,
    std::string_view comp
)
{
    auto p1 = glox::path(glox::path_view(base));
    auto p2 = p1 / glox::path_view(next);
    return p2 == glox::path_view(comp)
        && p1 == glox::path_view(base); // Ensure base is unmodified
}

// --- Static Asserts ---

// path_view: Absolute checks
static_assert(test_pv_is_absolute("/", true));
static_assert(test_pv_is_absolute("/usr/bin", true));
static_assert(test_pv_is_absolute("src/vfs.cpp", false));
static_assert(test_pv_is_absolute("", false));

// path_view: Filename parsing
static_assert(test_pv_filename("/usr/local/bin/kernel.elf", "kernel.elf"));
static_assert(test_pv_filename("vfs.cpp", "vfs.cpp"));
static_assert(test_pv_filename("/", ""));

// path_view: Parent path parsing
static_assert(test_pv_parent("/usr/local/bin/kernel.elf", "/usr/local/bin"));
static_assert(test_pv_parent("src/fs/vfs.cpp", "src/fs"));
static_assert(test_pv_parent("/usr", "/"));
static_assert(test_pv_parent("/", "/"));

// path_view: Extension parsing
static_assert(test_pv_extension("/usr/local/bin/kernel.elf", ".elf"));
static_assert(
    test_pv_extension("archive.tar.gz", ".gz")
); // Multiple dots, takes last
static_assert(
    test_pv_extension(".config", "")
); // Hidden files have no extension
static_assert(test_pv_extension("Makefile", "")); // No extension

// path_view: Stem parsing
static_assert(test_pv_stem("/usr/local/bin/kernel.elf", "kernel"));
static_assert(test_pv_stem("archive.tar.gz", "archive.tar"));
static_assert(test_pv_stem(".config", ".config"));
static_assert(test_pv_stem("Makefile", "Makefile"));

// path: Append / Concatenation
static_assert(
    test_path_append("/home/user", "documents", "/home/user/documents")
);
static_assert(
    test_path_append("/var/log/", "dmesg", "/var/log/dmesg")
); // Should not double slash
static_assert(
    test_path_append("/var/log", "/etc/passwd", "/etc/passwd")
); // Absolute RHS replaces LHS
static_assert(test_path_append("", "src", "src"));

// path: Operator /
static_assert(test_path_operator_slash("/tmp", "test.txt", "/tmp/test.txt"));
static_assert(test_path_operator_slash("/tmp/", "test.txt", "/tmp/test.txt"));

// Optional: doctest integration for runtime execution of the same checks
TEST_CASE("glox::path utility basics")
{
    CHECK(glox::path_view("/usr/bin").is_absolute() == true);
    CHECK(glox::path_view("src").filename() == "src");

    glox::path p { "/usr" };
    p /= "local";
    CHECK(p == "/usr/local");
}

TEST_CASE("glox::path runtime append and operator/")
{
    SUBCASE("Append relative path without trailing slash")
    {
        glox::path p { "/home/user" };
        p /= glox::path_view("documents");
        CHECK(p == glox::path_view("/home/user/documents"));
    }

    SUBCASE(
        "Append relative path with trailing slash (should not double slash)"
    )
    {
        glox::path p { "/var/log/" };
        p /= glox::path_view("dmesg");
        CHECK(p == glox::path_view("/var/log/dmesg"));
    }

    SUBCASE("Append absolute path (replaces LHS)")
    {
        glox::path p { "/var/log" };
        p /= glox::path_view("/etc/passwd");
        CHECK(p == glox::path_view("/etc/passwd"));
    }

    SUBCASE("Append to empty path")
    {
        glox::path p { "" };
        p /= glox::path_view("src");
        CHECK(p == glox::path_view("src"));
    }

    SUBCASE("Operator / without trailing slash")
    {
        glox::path p1 { "/tmp" };
        glox::path p2 = p1 / glox::path_view("test.txt");
        CHECK(p2 == glox::path_view("/tmp/test.txt"));
        CHECK(p1 == glox::path_view("/tmp")); // Ensure base is unmodified
    }

    SUBCASE("Operator / with trailing slash")
    {
        glox::path p1 { "/tmp/" };
        glox::path p2 = p1 / glox::path_view("test.txt");
        CHECK(p2 == glox::path_view("/tmp/test.txt"));
        CHECK(p1 == glox::path_view("/tmp/")); // Ensure base is unmodified
    }
}
