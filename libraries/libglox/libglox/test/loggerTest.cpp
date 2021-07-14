#include "glox/logger.hpp"
#include <algorithm>
#include <cstdio>

#define gloxLog(...) out, __VA_ARGS__


struct testest
{
};

inline auto format(char* buffer, testest a)
{
	const char* disgust = "Disgusting";
	std::copy(disgust, disgust + 18, buffer);
	return (size_t)18;
}

struct testStream : glox::bStream
{
	char buffer[256];
};

void write(testStream& st, size_t s)
{
	std::fwrite(st.buffer, 1, s, stdout);
}

void write(testStream&, const char* str, size_t s)
{
	std::fwrite(str, 1, s, stdout);
}

testStream out;

int main()
{
	gloxLog("Hello World!\nToday is " __TIME__
			"\nInt max64 = ",
			INT64_MAX, "\nInt min64 = ", INT64_MIN,
			"\n-1 = ", -1, "\n0 = ", 0, "Hopefully\n", testest{});
}