#pragma once
#include "glox/logger.hpp"
namespace glox
{
	struct logStream : glox::bStream
	{
		char buffer[256*2];
		template<typename... args>
		void operator()(args... Args)
		{
			((void)(*this,Args),...);
		}
	};
	void write(logStream&, const char* str, size_t s);
	//void write(logStream&, const char* str);
	extern glox::logStream outStream;
} // namespace glox

using glox::outStream;




#define gloxLog(...) glox::outStream, __VA_ARGS__
#define gloxLogln(...) glox::outStream, __VA_ARGS__, '\n'
#ifdef DEBUGBUILD
	#define gloxDebugLog(...) gloxLog(__VA_ARGS__)
	#define gloxDebugLogln(...) gloxLogln(__VA_ARGS__)
#else
	#define gloxDebugLog(...) ((void)0)
	#define gloxDebuglogln(...) ((void)0)
#endif
