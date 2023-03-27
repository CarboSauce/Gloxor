#pragma once
#ifdef GLOXTESTING
#define TEST
#endif
namespace gx {
struct Ktest {
	const char* name;
	bool (*const init)();
};
} // namespace gx

#ifdef GLOXTESTING
#define registerTest(name, fnc)                                                 \
	[[gnu::used, gnu::section(".module.test")]] static gx::Ktest _testptr_##fnc \
	{                                                                           \
		name, &fnc                                                              \
	}
#define KTEST_EXPECT(expr)                                                   \
	do {                                                                     \
		gloxPrintln("EXPECT(" #expr ") = ", (expr) ? "Success" : "Failure"); \
	} while (0)
#define KTEST_ASSERT(expr)                                 \
	do {                                                   \
		if (!(expr)) {                                     \
			gloxPrintln("KTEST_ASSERT(" #expr ") failed"); \
			return false;                                  \
		}                                                  \
	} while (0)
#else
#define registerTest(name, fnc)
#define KTEST_EXPECT(expr)
#define KTEST_ASSERT(expr)
#endif