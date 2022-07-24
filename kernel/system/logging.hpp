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
	fatal,
	trace,
	debug,
};

void write(logStream&, const char* str, size_t s);
// void write(logStream&, const char* str);
extern glox::logStream outStream;
inline logLevel logLevelCap = logLevel(LOGLEVEL);
} // namespace glox

using glox::logLevel;
using glox::outStream;
#define gloxPrint(...) glox::outStream, __VA_ARGS__
#define gloxPrintln(...) glox::outStream, __VA_ARGS__, '\n'
#define gloxLog(level, ...) (glox::logLevelCap < level ? (void)0 : (void)(glox::outStream, __VA_ARGS__))
#define gloxLogln(level, ...) (glox::logLevelCap < level ? (void)0 : (void)(glox::outStream, __VA_ARGS__, '\n'))
#define gloxFatalLog(...) gloxLog(glox::logLevel::fatal, __VA_ARGS__)
#define gloxFatalLogln(...) gloxLogln(glox::logLevel::fatal, __VA_ARGS__)
#define gloxTraceLog(...) gloxLog(glox::logLevel::trace, __VA_ARGS__)
#define gloxTraceLogln(...) gloxLogln(glox::logLevel::trace, __VA_ARGS__)
#define gloxDebugLog(...) gloxLog(glox::logLevel::debug, __VA_ARGS__)
#define gloxDebugLogln(...) gloxLogln(glox::logLevel::debug, __VA_ARGS__)
