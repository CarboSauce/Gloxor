#include "logging.hpp"
#include "asm/e9.hpp"
#include "danger.hpp"
#include "system/terminal.hpp"
namespace glox
{
void exec_assert(const char* message, const char* file, const char* line)
{
	glox::outStream, "Assertion failed: \n (", message, ")\n ",
		 file, ":", line, '\n';
	glox::kernel_panic();
}

void write(glox::LogStream& out, const char* str, size_t s)
{
	glox::term::write_str(str, s);
	print_e9(str, s);
	(void)out;
}

LogStream outStream;

} // namespace glox
