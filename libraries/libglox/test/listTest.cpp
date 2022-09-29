#include "glox/linkedlist.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
struct test_struct
{
	int x;
	glox::list_node list_node;
};
struct test2_struct
{
	int x;
	glox::fwd_list_node<test2_struct> fwd_list_node;
};

using testlist = glox::intrusive_list<test_struct>;
using test2list = glox::intrusive_fwd_list<test2_struct>;
auto asmtest(testlist& a)
{
	return a.back().x;
}

int main()
{
	testlist l{};
	l.push_back(new test_struct{2});
	l.insert(begin(l),new test_struct{1});
	l.insert(end(l),new test_struct{3});
	int i = 1;
	auto it2 = l.begin();
	testlist::const_iterator it3 = it2;
	delete static_cast<testlist::iterator::pointer>(l.erase(l.begin()));
	l.push_front(new test_struct{1});
	for (const auto& it : l)
	{
		//assert(it.x == i++);
		if (i == 10) abort();
		printf("%d\n",it.x);
	}

	l.clear([](test_struct* p)
	{
		delete p;
	});
	puts("Success");
}
