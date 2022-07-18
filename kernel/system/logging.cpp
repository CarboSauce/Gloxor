#include "logging.hpp"
#include "danger.hpp"
#include "drivers/e9.hpp"
#include "system/terminal.hpp"
namespace glox
{
void execAssert(const char* message, const char* file, const char* line)
{
	glox::outStream, "Assertion failed: \n (", message, ")\n ",
			  file, ":", line, '\n';
	glox::kernelPanic();
}

void write(glox::logStream& out, const char* str, size_t s)
{
	glox::term::writeStr(str, s);
	printE9(str, s);
	(void)out;
}

logStream outStream;

} // namespace glox
