#include "glox/logger.hpp"
#include <algorithm>
#include <Windows.h>
#include <cstring>
#include <iostream>

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

constexpr size_t bufSize = (4096);

struct testStream : glox::bStream
{
	char _buffer[bufSize];
	char* buffer = _buffer;
	~testStream();
};

// void write(testStream& st, size_t s)
// {
// 	std::fwrite(st.buffer, 1, s, stdout);
// }

struct based
{
	based()
	{
	
#ifdef ASYNC_TEST
		con=CreateFile("CONOUT$",GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,NULL);
#else
		con=CreateFile("CONOUT$",GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
#endif
		if (con == nullptr)
		{
			std::cout << "epic fail";
			abort();
		}
	}

	HANDLE con;
	#ifdef ASYNC_TEST
	OVERLAPPED overl;
	#endif
};

based bogus{};


testStream out;


#ifdef ASYNC_TEST
char flushbuf[bufSize]{};
void flush(testStream& ouut)
{
	DWORD bajto;
	memcpy((void*)flushbuf, out._buffer, out.buffer - out._buffer );
	while(!HasOverlappedIoCompleted((volatile OVERLAPPED*)&bogus.overl));
	WriteFile(bogus.con,(void*)flushbuf,(DWORD)(out.buffer - out._buffer),NULL,&bogus.overl);

	out.buffer = out._buffer;
}
#else
void flush(testStream& ouut)
{
	DWORD bajto;
	WriteFile(bogus.con,ouut._buffer,(DWORD)(out.buffer - out._buffer),&bajto,NULL);
	out.buffer = out._buffer;
}
#endif


void write(testStream& ok, const char* str, size_t s)
{
	//std::fwrite(str, 1, s, stdout);
	if ((size_t)(ok.buffer-ok._buffer + s) >= bufSize)
		flush(ok);
	memcpy(ok.buffer,str,s);
	ok.buffer+=s;
}

testStream::~testStream()
{
	flush(*this);
}
int main()
{
   for (size_t i = 0; i < 1'000'000; ++i)
   {
      gloxLog("Omegalul you retarded fastio ",
		i,
		'\n');
   }
}