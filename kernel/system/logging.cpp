#include "logging.hpp"
#include "asm/e9.hpp"
#include "danger.hpp"
#include "system/terminal.hpp"
void glox::exec_assert(const char* message, const char* file, const char* line)
{
	gx::outStream, "Assertion failed: \n (", message, ")\n ",
		file, ":", line, '\n';
	gx::kernel_panic();
}
namespace gx {

void write(gx::LogStream& out, const char* str, size_t s)
{
	gx::term::write_str(str, s);
	print_e9(str, s);
	(void)out;
}

LogStream outStream;

} // namespace gx
