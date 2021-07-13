#pragma once
#include "glox/logger.hpp"
#include "string.h"
namespace glox
{
	struct logStream : glox::bStream
	{
		char buffer[256];
	/* 	template<typename... args>
		void operator()(args... Args)
		{
			((void)(*this,Args),...);
		} */
	};
	void write(logStream&, size_t s);
	void write(logStream&, const char* str);
	extern glox::logStream outStream;
	void execAssert(const char* message, const char* file, const char* line);
} // namespace glox

using glox::outStream;


#define _mSTRINGIFY(x) _implSTRINGIFY(x)
#define _implSTRINGIFY(x) #x

#define gloxLog(...) glox::outStream, __VA_ARGS__
#define gloxLogln(...) glox::outStream, __VA_ARGS__, '\n'
#ifdef DEBUGBUILD
	#define gloxDebugLog(...) gloxLog(__VA_ARGS__)
	#define gloxDebugLogln(...) gloxLogln(__VA_ARGS__)
	#define gloxAssert(cond, ...) (void)((!!(cond)) || \
	(glox::execAssert(#cond" " __VA_ARGS__,__FILE__,_mSTRINGIFY(__LINE__)),0))
#else
	#define gloxDebugLog(...) ((void)0)
	#define gloxDebuglogln(...) ((void)0)
	#define gloxAssert(cond, ...) ((void)0)
#endif
