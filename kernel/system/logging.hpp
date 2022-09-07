#pragma once
#include "glox/logger.hpp"
#include "system/logging.hpp"
namespace gx
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
extern gx::LogStream outStream;
inline LogLevel LogLevelCap = LogLevel(LOG_LEVEL);
} // namespace gx

using gx::LogLevel;
using gx::outStream;
#define gloxPrint(...) gx::outStream, __VA_ARGS__
#define gloxPrintln(...) gx::outStream, __VA_ARGS__, '\n'
#define gloxLog(level, ...) (gx::LogLevelCap < level ? (void)0 : (void)(gx::outStream, __VA_ARGS__))
#define gloxLogln(level, ...) (gx::LogLevelCap < level ? (void)0 : (void)(gx::outStream, __VA_ARGS__, '\n'))
#define gloxFatalLog(...) gloxLog(gx::LogLevel::fatal, __VA_ARGS__)
#define gloxFatalLogln(...) gloxLogln(gx::LogLevel::fatal, __VA_ARGS__)
#define gloxTraceLog(...) gloxLog(gx::LogLevel::trace, __VA_ARGS__)
#define gloxTraceLogln(...) gloxLogln(gx::LogLevel::trace, __VA_ARGS__)
#define gloxDebugLog(...) gloxLog(gx::LogLevel::debug, __VA_ARGS__)
#define gloxDebugLogln(...) gloxLogln(gx::LogLevel::debug, __VA_ARGS__)
