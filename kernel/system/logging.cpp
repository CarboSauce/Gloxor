#include "logging.hpp"
#include "drivers/e9.hpp"
#include "danger.hpp"
namespace glox
{

	void execAssert(const char* message, const char* file, const char* line)
   {
      gloxLog("Assertion failed: \n (", message, ")\n ",
		file, ":", line,'\n');

      glox::kernelPanic();
   }
	void write(glox::logStream& out, size_t s)
	{
		printE9(out.buffer, s);
	}
	void write(glox::logStream&, const char* str)
	{
		printE9(str);
	}
	logStream outStream;

} // namespace glox
