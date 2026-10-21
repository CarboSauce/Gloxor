export module gloxor.system.logging;
import glox;
import std.compat;

#include "glox/macros.hpp"

export namespace gx {
class LogStream : ::glox::b_stream
{

public:
    char buffer[512];
    uint32_t curLogLevel;
    template <typename... args>
    void operator()(args... Args)
    {
        ((void)(*this, Args), ...);
    }
};

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
        buffer[i++] = c;

        if (c == '\n')
            flush();

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

void write(LogStream&, const char* str, size_t s);

extern gx::LogStream outStream;
inline gx::BufferStream logStream;
inline LogLevel LogLevelCap = LogLevel(LOG_LEVEL);

template <typename... Args>
void print(std::string_view fmt, Args&&... args)
{
    glox::format_to(logStream, fmt, FORWARD(args)...);
}
template <typename... Args>
void println(std::string_view fmt, Args&&... args)
{
    glox::format_to(logStream, fmt, FORWARD(args)...);
    logStream.write('\n');
    logStream.flush();
}
template <typename... Args>
void log(LogLevel lvl, std::string_view fmt, Args&&... args)
{
    if (lvl < LogLevelCap)
        glox::format_to(logStream, fmt, FORWARD(args)...);
}
template <typename... Args>
void logln(LogLevel lvl, std::string_view fmt, Args&&... args)
{
    if (lvl < LogLevelCap) {
        glox::format_to(logStream, fmt, FORWARD(args)...);
        logStream.write('\n');
        logStream.flush();
    }
}
template <typename... Args>
void log_fatal(std::string_view fmt, Args&&... args)
{
    if (LogLevel::fatal < LogLevelCap)
        glox::format_to(logStream, fmt, FORWARD(args)...);
}
template <typename... Args>
void log_trace(std::string_view fmt, Args&&... args)
{
    if (LogLevel::trace < LogLevelCap) {
        glox::format_to(logStream, fmt, FORWARD(args)...);
    }
}
template <typename... Args>
void log_debug(std::string_view fmt, Args&&... args)
{
    if (LogLevel::debug < LogLevelCap)
        glox::format_to(logStream, fmt, FORWARD(args)...);
}
} // namespace gx

using gx::LogLevel;
using gx::logStream;
using gx::outStream;
