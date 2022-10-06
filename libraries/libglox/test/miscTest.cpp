#include <new>
#include "glox/result.hpp"
#include <type_traits>
#include <cstdio>
#include <string>

//enum class option_type : bool
//{
//	Empty,
//};
//using option_type::Empty;
using option_type = bool;
using optional_int = glox::result<int,bool>;
constexpr static bool Empty = true;

constexpr auto test_consxpr()
{
	//auto t = glox::result<int,option_type>::from_err(Empty);
	optional_int t(5);
	auto t2 = optional_int::from_val(10);
	return RVALUE(t).unwrap();
}
struct NonTrivial
{
	NonTrivial(const NonTrivial&) {}
	NonTrivial(NonTrivial&&) {}
	~NonTrivial() {}
};
using nontrivial_res = glox::result<NonTrivial, option_type>;

static constexpr auto res = test_consxpr();
static_assert(std::is_trivially_destructible_v<optional_int>);
static_assert(not std::is_trivially_destructible_v<nontrivial_res>);
static_assert(std::is_trivially_copyable_v<optional_int>);

struct St
{
	St(){printf("St(%p)\n",this);}
	~St(){printf("~St(%p)\n",this);}
	St(const St&){printf("St&(%p)\n",this);}
	St(St&&){printf("St&&(%p)\n",this);}
	St& operator=(const St&){printf("=St&(%p)\n",this);return *this;}
	St& operator=(St&&) {printf("=St&&(%p)\n",this);return *this;}
};
using optional_st = glox::result<St,option_type>;
using opt_string = glox::result<std::string,option_type>;
glox::result<int> test_res(int a,int b)
{
	if (b != 0) return glox::result(a/b);
	return glox::result<int>::from_err(glox::none);

}
auto test_sus(glox::result<uint64_t> a)
{
	if (a.has_value()) return a.val();
	else throw 5;
}

int main()
{
	optional_st st1(St{});
	puts("S2.");
	optional_st st2(st1);
	puts("S3.");
	st2 = st1;
	puts("STRINGS");
	opt_string s1(std::string("Hello World, Long String"));
	puts("COPY CTOR");
	{
		auto s2{s1};
		s2.val()[5] = '!';
		printf("s2: %s\n",s2.val().c_str());
		puts("COPY ASSIGNMENT");
		s2 = s1;
		printf("s2: %s\n",s2.val().c_str());
	}
	printf("s1: %s\n",s1.val().c_str());

}
