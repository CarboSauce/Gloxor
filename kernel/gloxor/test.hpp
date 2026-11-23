#pragma once
#ifdef GLOXTESTING
#define TEST
#endif

extern "C++"
{
namespace gx {
    struct Ktest
    {
        glox::string name;
        bool (*const init)();
    };
    struct KtestContext
    {
        int assert_counter = 0;
        int success_counter = 0;
        int failed_counter = 0;
        void run();
    };
} // namespace gx
}

#ifdef GLOXTESTING
extern "C++"
{
namespace gx {
    inline KtestContext ktestContext;
}
}
#define REGISTER_TEST(name, fnc)                \
    [[gnu::used, gnu::section(".module.test")]] \
    static gx::Ktest _testptr_##fnc             \
    {                                           \
        name, &fnc                              \
    }
#define KTEST_EXPECT(expr)                                            \
    do {                                                              \
        gx::ktestContext.assert_counter++;                            \
        auto _tmp = (expr);                                           \
        if (!_tmp) {                                                  \
            gx::ktestContext.failed_counter++;                        \
            gx::println(                                              \
                "EXPECT(%)= %", #expr, (_tmp) ? "Success" : "Failure" \
            );                                                        \
        } else {                                                      \
            gx::ktestContext.success_counter++;                       \
        }                                                             \
    } while (0)
#define KTEST_ASSERT(expr)                                 \
    do {                                                   \
        gx::ktestContext.assert_counter++;                 \
        if (!(expr)) {                                     \
            gx::ktestContext.failed_counter++;             \
            gx::println("KTEST_ASSERT(" #expr ") failed"); \
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
