#include "glox/linkedlist.hpp"
#include <cassert>
#include <cstdio>
#include <iterator>
struct test_struct
{
	int x,y;
	glox::list_node<test_struct> list_node;
};
struct test2_struct
{
	int x;
	glox::fwd_list_node<test2_struct> fwd_list_node;
};

using testlist = glox::intrusive_list<test_struct>;
using test2list = glox::intrusive_fwd_list<test2_struct>;

int main()
{
	testlist l1;
	for (int i = 0; i < 1000; i++)
	{
		l1.push_back(new test_struct{i,i*2});
	}
	int i = 0;
	for (auto it = l1.begin(); it != l1.end(); ++it)
	{
		assert(it->x == i && it->y == i*2);
		i++;
	}

	l1.clear([](auto p){delete p;});
	assert(l1.begin() == testlist::iterator(nullptr));
	test2list l2;
	for (int i = 0; i < 1000; i++)
	{
		l2.push_front(new test2_struct{999-i});
	}
	i = 0;
	for (auto it = l2.begin(); it != l2.end(); ++it)
	{
		assert(it->x == 999-i);
		i++;
	}
	l2.clear([](auto p){delete p;});
	assert(l2.begin() == test2list::iterator(nullptr));
	puts("Success!");
}
