#pragma once

namespace glox
{
   void execAssert(const char* message, const char* file, const char* line);
}

#ifdef DEBUGBUILD
   #define _mSTRINGIFY(x) _implSTRINGIFY(x)
   #define _implSTRINGIFY(x) #x
	#define gloxAssert(cond, ...) (void)((!!(cond)) || \
	(glox::execAssert(#cond" " __VA_ARGS__,__FILE__,_mSTRINGIFY(__LINE__)),0))
   #define gloxUnreachable() gloxAssert(false,"Unreachable is hit");__builtin_unreachable() 

#else
	#define gloxAssert(cond, ...) ((void)0)
   #define gloxUnreachable() ((void)0)
#endif
