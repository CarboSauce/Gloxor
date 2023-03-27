#include "glox/vector.hpp"
#include <cstdio>
#include <cassert>
#include <vector>
struct St
{
	St(){printf("St(%p)\n",this);}
	~St(){printf("~St(%p)\n",this);}
	St(const St&){printf("St&(%p)\n",this);}
	St(St&&){printf("St&&(%p)\n",this);}
	St& operator=(const St&){printf("=St&(%p)\n",this);return *this;}
	St& operator=(St&&) {printf("=St&&(%p)\n",this);return *this;}
};
static_assert(sizeof(glox::vector<St>) == sizeof(St*) + sizeof(size_t)*2);
auto test(glox::result<std::vector<int>>& a){
	if (a.has_value()) return RVALUE(a).unwrap();
	throw 5;
}
int main()
{
	auto a = glox::vector<int>(10);
	for (int i = 0; i < 10; ++i)
	{
		auto tmp = a.emplace_back();
		assert(tmp);
	}
	for (const auto& it : a)
	{
		printf("%d\n", it);
	}
	auto b = RVALUE(a);
	puts("b = std::move(a)");
	for (const auto& it : a)
	{
		printf("%d\n", it);
	}
	puts("now b");
	for (const auto& it : b)
	{
		printf("%d\n", it);
	}
	puts("Success");
}
