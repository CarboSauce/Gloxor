export module gloxor.system.logging;
import glox;
import std.compat;

#include "glox/macros.hpp"

export namespace gx {
class BufferStream
{
    int i = 0;
    char buffer[512];

public:
    size_t write(char c)
    {
        if (i == sizeof(buffer)) {
            flush();
            i = 0;
        }
        if (c != '\n') {
            buffer[i++] = c;
        } else {
            buffer[i++] = '\r';
            buffer[i++] = '\n';
            flush();
        }

        return i;
    }

    size_t flush();
};
enum class LogLevel
{
    fatal,
    trace,
    debug,
};

inline gx::BufferStream logStream;
inline LogLevel LogLevelCap = LogLevel(LOG_LEVEL);

template <typename... Args>
void print(glox::format_string<Args...> fmt, Args&&... args)
{
    glox::format_to(logStream, fmt, FORWARD(args)...);
}
template <typename... Args>
void println(glox::format_string<Args...> fmt, Args&&... args)
{
    glox::format_to(logStream, fmt, FORWARD(args)...);
    logStream.write('\r');
    logStream.write('\n');
    logStream.flush();
}
template <typename... Args>
void log(LogLevel lvl, glox::format_string<Args...> fmt, Args&&... args)
{
    if (lvl < LogLevelCap)
        glox::format_to(logStream, fmt, FORWARD(args)...);
}
template <typename... Args>
void logln(LogLevel lvl, glox::format_string<Args...> fmt, Args&&... args)
{
    if (lvl < LogLevelCap) {
        glox::format_to(logStream, fmt, FORWARD(args)...);
        logStream.write('\r');
        logStream.write('\n');
        logStream.flush();
    }
}
template <typename... Args>
void log_fatal(glox::format_string<Args...> fmt, Args&&... args)
{
    if (LogLevel::fatal < LogLevelCap)
        glox::format_to(logStream, fmt, FORWARD(args)...);
}
template <typename... Args>
void log_trace(glox::format_string<Args...> fmt, Args&&... args)
{
    if (LogLevel::trace < LogLevelCap) {
        glox::format_to(logStream, fmt, FORWARD(args)...);
    }
}
template <typename... Args>
void log_debug(glox::format_string<Args...> fmt, Args&&... args)
{
    if (LogLevel::debug < LogLevelCap)
        glox::format_to(logStream, fmt, FORWARD(args)...);
}
template <typename... Args>
void logln_fatal(glox::format_string<Args...> fmt, Args&&... args)
{
    if (LogLevel::fatal < LogLevelCap) {
        glox::format_to(logStream, fmt, FORWARD(args)...);
        logStream.write('\r');
        logStream.write('\n');
        logStream.flush();
    }
}
template <typename... Args>
void logln_trace(glox::format_string<Args...> fmt, Args&&... args)
{
    if (LogLevel::trace < LogLevelCap) {
        glox::format_to(logStream, fmt, FORWARD(args)...);
        logStream.write('\r');
        logStream.write('\n');
        logStream.flush();
    }
}
template <typename... Args>
void logln_debug(glox::format_string<Args...> fmt, Args&&... args)
{
    if (LogLevel::debug < LogLevelCap) {
        glox::format_to(logStream, fmt, FORWARD(args)...);
        logStream.write('\r');
        logStream.write('\n');
        logStream.flush();
    }
}
} // namespace gx

using gx::LogLevel;
using gx::logStream;
