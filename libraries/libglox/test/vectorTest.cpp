#include "glox/vector.hpp"
#include <cstdio>
#include <cassert>

int main()
{
	glox::vector<int> a;
	for (int i = 0; i < 10000; ++i)
	{
		assert(a.emplace_back(i));
	}
	for (const auto& it : a)
	{
		printf("%d ", it);
	}
	auto b = RVALUE(a);
	puts("Success");
}
