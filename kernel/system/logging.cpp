module gloxor.system.logging;
import arch_asm;
import gloxor.system;
import std;
import arch_asm.e9;

#include "glox/macros.hpp"

namespace glox {
extern "C++" void exec_assert(const char* message, std::source_location loc)
{
    gx::outStream, "Assertion failed: \n (", message, ")\n ", loc.file_name(),
        ":", loc.line(), '\n';
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
void write(gx::LogStream& out, const char* str, size_t s)
{
    gx::term::write_str(str, s);
    print_e9(str, s);
    (void)out;
}

LogStream outStream;

} // namespace gx
