#pragma once
#include "glox/logger.hpp"
#include "system/logging.hpp"
namespace glox
{
struct LogStream : glox::b_stream
{
	char buffer[512];
	uint32_t curLogLevel;
	template <typename... args>
	void operator()(args... Args)
	{
		((void)(*this, Args), ...);
	}
};
enum class LogLevel
{
	fatal,
	trace,
	debug,
};

void write(LogStream&, const char* str, size_t s);
// void write(logStream&, const char* str);
extern glox::LogStream outStream;
inline LogLevel LogLevelCap = LogLevel(LOG_LEVEL);
} // namespace glox

using glox::LogLevel;
using glox::outStream;
#define gloxPrint(...) glox::outStream, __VA_ARGS__
#define gloxPrintln(...) glox::outStream, __VA_ARGS__, '\n'
#define gloxLog(level, ...) (glox::LogLevelCap < level ? (void)0 : (void)(glox::outStream, __VA_ARGS__))
#define gloxLogln(level, ...) (glox::LogLevelCap < level ? (void)0 : (void)(glox::outStream, __VA_ARGS__, '\n'))
#define gloxFatalLog(...) gloxLog(glox::LogLevel::fatal, __VA_ARGS__)
#define gloxFatalLogln(...) gloxLogln(glox::LogLevel::fatal, __VA_ARGS__)
#define gloxTraceLog(...) gloxLog(glox::LogLevel::trace, __VA_ARGS__)
#define gloxTraceLogln(...) gloxLogln(glox::LogLevel::trace, __VA_ARGS__)
#define gloxDebugLog(...) gloxLog(glox::LogLevel::debug, __VA_ARGS__)
#define gloxDebugLogln(...) gloxLogln(glox::LogLevel::debug, __VA_ARGS__)
