export module gloxor.system.logging;
import glox;
import std.compat;

export namespace gx {
struct LogStream : ::glox::b_stream
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
