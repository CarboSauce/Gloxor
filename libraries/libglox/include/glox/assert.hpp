#pragma once

namespace glox
{
[[noreturn]] void execAssert(const char* message, const char* file, const char* line);
}

#define gloxUnimplemented() glox::execAssert("The function is unimplemented", __FILE__, _mSTRINGIFY(__LINE__))
#ifdef DEBUG
	#define _mSTRINGIFY(x) _implSTRINGIFY(x)
	#define _implSTRINGIFY(x) #x
	#define gloxAssert(cond, ...) (void)((!!(cond)) || (glox::execAssert(#cond " " __VA_ARGS__, __FILE__, _mSTRINGIFY(__LINE__)), 0))
	#define gloxUnreachable()                      \
		gloxAssert(false, "unreachable() invoked"); \
		__builtin_unreachable()

#else
	#define gloxAssert(cond, ...) ((void)0)
	#define gloxUnreachable() __builtin_unreachable();
#endif
