#include "glox/linkedlist.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
struct test_struct
{
	int x;
	glox::list_node list_node;
	glox::list_node list2_node;
};
struct test2_struct
{
	int x;
	glox::fwd_list_node<test2_struct> fwd_list_node;
};

using test1list = glox::intrusive_list<test_struct>;
using test2list = glox::intrusive_list<test_struct, &test_struct::list2_node>;
using testfwdlist = glox::intrusive_fwd_list<test2_struct>;

int main()
{
	test1list l1{};
	test2list l2{};
	test_struct s[6]{{1}, {2}, {3}, {4}, {5}, {6}};
	puts("EMPTY LIST");
	for (const auto& it : l1)
	{
		printf("%d\n", it.x);
	}
	puts("END EMPTY LIST");
	for (int i = 0; i != 6; ++i)
	{
		l1.push_back(s + i);
	}
	for (int i = 0; i != 6; i += 2)
		l2.push_back(s + i);
	puts("L1");
	for (const auto& it : l1)
	{
		printf("%d\n", it.x);
	}
	puts("L2");
	for (const auto& it : l2)
	{
		printf("%d\n", it.x);
	}
	l2.clear();
	puts("L1 after L2 clear");
	for (const auto& it : l1)
	{
		printf("%d\n", it.x);
	}
	l1.clear();
	puts("L1 should be empty");
	for (auto tmp = l1.begin(); tmp != l1.end(); ++tmp)
	{
		printf("%d\n", tmp->x);
	}
	puts("Success");
}
