#include "logging.hpp"
#include "drivers/e9.hpp"
#include "danger.hpp"
#include "gloxor/graphics.hpp"
extern glox::framebuffer con;
namespace glox
{
	void execAssert(const char* message, const char* file, const char* line)
   {
      gloxLog("Assertion failed: \n (", message, ")\n ",
		file, ":", line,'\n');

      glox::kernelPanic();
   }
	void write(glox::logStream& out, const char* str, size_t s)
	{
		printE9(str, s);
		(void)out;
	}

	logStream outStream;
} // namespace glox
