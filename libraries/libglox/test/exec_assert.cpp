#ifndef USE_MODULES
#include <cstdio>
#include <cstdlib>
#include <source_location>
#else
import std;
import glox;
#endif
namespace glox {
[[noreturn]] void exec_assert(const char* message, std::source_location loc)
{
    std::printf(
        "Assert hit\nMessage:\nFunction: %s\n%s\nFile: %s\nLine: %d\n",
        message,
        loc.function_name(),
        loc.file_name(),
        loc.line()
    );
    std::exit(1);
}
} // namespace glox
