module gloxor.system.logging;
import arch_asm;
import gloxor.system;
import std;
import arch_asm.e9;

#include "glox/macros.hpp"

namespace glox {
extern "C++" void exec_assert(const char* message, std::source_location loc)
{
    gx::println(
        "Assertion failed: \r\n (%)\r\n %:%\r\n",
        message,
        loc.file_name(),
        loc.line()
    );
    gx::kernel_panic();
}
} // namespace glox
namespace gx {
size_t BufferStream::flush()
{
    gx::term::write_str(buffer, i);
    print_e9(buffer, i);
    auto tmp = i;
    i = 0;
    return tmp;
}

} // namespace gx
