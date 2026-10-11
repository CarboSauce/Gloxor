#pragma once
#define gloxPrint(...) gx::outStream, __VA_ARGS__
#define gloxPrintln(...) gx::outStream, __VA_ARGS__, '\n'
#define gloxLog(level, ...) \
    (gx::LogLevelCap < level ? (void)0 : (void)(gx::outStream, __VA_ARGS__))
#define gloxLogln(level, ...)          \
    (gx::LogLevelCap < level ? (void)0 \
                             : (void)(gx::outStream, __VA_ARGS__, '\n'))
#define gloxFatalLog(...) gloxLog(gx::LogLevel::fatal, __VA_ARGS__)
#define gloxFatalLogln(...) gloxLogln(gx::LogLevel::fatal, __VA_ARGS__)
#define gloxTraceLog(...) gloxLog(gx::LogLevel::trace, __VA_ARGS__)
#define gloxTraceLogln(...) gloxLogln(gx::LogLevel::trace, __VA_ARGS__)
#define gloxDebugLog(...) gloxLog(gx::LogLevel::debug, __VA_ARGS__)
#define gloxDebugLogln(...) gloxLogln(gx::LogLevel::debug, __VA_ARGS__)
