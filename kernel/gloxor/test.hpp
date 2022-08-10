#pragma once
#ifdef GLOXTESTING
	#define TEST
#endif
namespace glox
{
struct ktest 
{
	const char* name;
	bool (*const init)();
};
} // namespace glox

#define registerTest(name,fnc) [[maybe_unused, gnu::used, gnu::section(".module.test")]] \
											 static glox::ktest _testptr_##fnc {name,&fnc}
#define KTEST_EXPECT(expr) do { gloxPrintln("EXPECT(" #expr ") = ",(expr)?"Success":"Failure");}while(0)
#define KTEST_ASSERT(expr) do { if (!(expr)){gloxPrintln("KTEST_ASSERT("#expr ") failed");return false;}}while(0)
