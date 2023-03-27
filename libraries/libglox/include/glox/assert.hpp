#pragma once

namespace glox
{
[[noreturn]] void exec_assert(const char* message, const char* file, const char* line);
}

#define gloxUnimplemented() glox::exec_assert("The function is unimplemented", __FILE__, _mSTRINGIFY(__LINE__))
#ifdef DEBUG
	#define _mSTRINGIFY(x) _implSTRINGIFY(x)
	#define _implSTRINGIFY(x) #x
	#define gloxDebugError(...) (glox::exec_assert(__VA_ARGS__, __FILE__, _mSTRINGIFY(__LINE__)))
	#define gloxAssert(cond, ...) (void)((!!(cond)) || (glox::exec_assert(#cond " " __VA_ARGS__, __FILE__, _mSTRINGIFY(__LINE__)), 0))
	#define gloxUnreachable()                      \
		gloxAssert(false, "unreachable() invoked"); \
		__builtin_unreachable()
#else
	#define gloxAssert(cond, ...) ((void)0)
	#define gloxUnreachable() __builtin_unreachable();
	#define gloxDebugError(...) ((void)0)
#endif
