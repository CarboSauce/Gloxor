#pragma once

#ifndef USE_MODULES
#include <source_location>
#endif

namespace glox {
extern "C++" [[noreturn]] void exec_assert(
    const char* message,
    std::source_location = std::source_location::current()
);
}

#define gloxUnimplemented() glox::exec_assert("The function is unimplemented")
#ifndef NDEBUG
#ifndef DEBUG
#define DEBUG
#endif
#endif
#ifdef DEBUG
#define GLOX_CONCAT_NX(a, b) a##b
#define GLOX_CONCAT(a, b) GLOX_CONCAT_NX(a, b)

#define GLOX_NUM_ARGS_MATCH(_1, _2, _3, _4, _5, _6, _7, _8, N, ...) N
#define GLOX_NUM_ARGS(...) \
    GLOX_NUM_ARGS_MATCH(__VA_ARGS__, 8, 7, 6, 5, 4, 3, 2, 1, 0)

#define GLOX_OVERLOAD(name, ...) GLOX_CONCAT(name, GLOX_NUM_ARGS(__VA_ARGS__))

#define GLOX_STRINGIFY(x) GLOX_IMPL_STRINGIFY(x)
#define GLOX_IMPL_STRINGIFY(x) #x

#define GLOX_ASSERT(...) GLOX_OVERLOAD(GLOX_ASSERT, __VA_ARGS__)(__VA_ARGS__)
#define GLOX_ASSERT1(cond)            \
    do {                              \
        if (!(cond)) {                \
            glox::exec_assert(#cond); \
        }                             \
    } while (0)
#define GLOX_ASSERT2(cond, msg)           \
    do {                                  \
        if (!(cond)) {                    \
            glox::exec_assert(#cond " "); \
        }                                 \
    } while (0)
#define GLOX_ASSERT3(cond, msg, loc)                \
    do {                                            \
        if (!(cond)) {                              \
            glox::exec_assert(#cond " " #msg, loc); \
        }                                           \
    } while (0)

#define gloxDebugError(...) \
    (glox::exec_assert(__VA_ARGS__, __FILE__, _mSTRINGIFY(__LINE__)))
#define gloxAssert(cond, ...) GLOX_ASSERT(cond, __VA_ARGS__)
#define gloxUnreachable()                          \
    gloxAssert(false, "unreachable code invoked"); \
    __builtin_unreachable()
#define GLOX_UNREACHABLE()                         \
    gloxAssert(false, "Unreachable code invoked"); \
    __builtin_unreachable()
#else
#define gloxAssert(cond, ...) ((void)0)
#define gloxUnreachable() __builtin_unreachable()
#define GLOX_UNREACHABLE() __builtin_unreachable()
#define gloxDebugError(...) ((void)0)
#endif
