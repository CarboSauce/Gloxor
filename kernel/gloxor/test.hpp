#pragma once
#ifdef GLOXTESTING
#define TEST
#endif

namespace gx {
struct Ktest
{
    std::string_view name;
    bool (*const init)();
};
extern "C"
{
extern gx::Ktest _moduleTesting[];
extern gx::Ktest _moduleTestingEnd[];
}
struct KtestContext
{
    int assert_counter = 0;
    int success_counter = 0;
    int failed_counter = 0;
    void run()
    {
        auto testCount = _moduleTestingEnd - _moduleTesting;
        gx::println("There are % tests", testCount);
        int passCounter = 0;
        for (auto it = _moduleTesting; it != _moduleTestingEnd; ++it) {
            using namespace std::string_view_literals;
            auto res = it->init();
            if (not res) {
                gx::println("Test % failed", it->name);
            }
            passCounter += res ? 1 : 0;
        }

        gx::println(
            "Tests: % passed; % failed;\r\n",
            passCounter,
            testCount - passCounter
        );
        gx::println(
            "Asserts: % count; % passed; % failed;\r\n",
            assert_counter,
            success_counter,
            failed_counter
        );
    }
};
} // namespace gx

#ifdef GLOXTESTING
namespace gx {
inline KtestContext ktestContext;
}
#define REGISTER_TEST(name, fnc)                \
    [[gnu::used, gnu::section(".module.test")]] \
    static gx::Ktest _testptr_##fnc             \
    {                                           \
        name, &fnc                              \
    }
#define KTEST_EXPECT(expr)                                              \
    do {                                                                \
        gx::ktestContext.assert_counter++;                              \
        auto _tmp = (expr);                                             \
        if (!_tmp) {                                                    \
            gx::ktestContext.failed_counter++;                          \
            gx::println(                                                \
                "EXPECT(" #expr ") = %", (_tmp) ? "Success" : "Failure" \
            );                                                          \
        } else {                                                        \
            gx::ktestContext.success_counter++;                         \
        }                                                               \
    } while (0)
#define KTEST_ASSERT(expr)                                 \
    do {                                                   \
        gx::ktestContext.assert_counter++;                 \
        if (!(expr)) {                                     \
            gx::ktestContext.failed_counter++ gx::println( \
                "KTEST_ASSERT(" #expr ") failed"           \
            );                                             \
            return false;                                  \
        } else {                                           \
            gx::ktestContext.success_counter++;            \
        }                                                  \
    } while (0)
#else
#define REGISTER_TEST(name, fnc)
#define KTEST_EXPECT(expr)
#define KTEST_ASSERT(expr)
#endif
