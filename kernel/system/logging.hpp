#pragma once
#include "glox/logger.hpp"
#include "system/logging.hpp"
namespace glox
{
struct logStream : glox::bStream
{
	char buffer[512];
	uint32_t curLogLevel;
	template <typename... args>
	void operator()(args... Args)
	{
		((void)(*this, Args), ...);
	}
};
enum class logLevel
{
	debug,
	trace,
	disable
};

void write(logStream&, const char* str, size_t s);
// void write(logStream&, const char* str);
extern glox::logStream outStream;
inline logLevel logLevelCap = logLevel(LOGLEVEL);
} // namespace glox

using glox::outStream;

#define gloxLog(level, ...) (glox::logLevelCap < level ? (void)0 : (void)(glox::outStream, __VA_ARGS__))
#define gloxLogln(level, ...) (glox::logLevelCap < level ? (void)0 : (void)(glox::outStream, __VA_ARGS__,'\n'))
#define gloxTraceLog(...) gloxLog(glox::logLevel::trace, __VA_ARGS__)
#define gloxTraceLogln(...) gloxLog(glox::logLevel::trace, __VA_ARGS__)
#define gloxDebugLog(...) gloxLog(glox::logLevel::debug, __VA_ARGS__)
#define gloxDebugLogln(...) gloxLogln(glox::logLevel::debug, __VA_ARGS__)
