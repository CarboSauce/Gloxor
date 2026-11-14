import glox;
import gloxor.system.logging;
import std;
#include "test.hpp"
extern "C"
{
extern gx::Ktest _moduleTesting[];
extern gx::Ktest _moduleTestingEnd[];
}

namespace gx {
extern "C++"
{
void KtestContext::run()
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
        "Tests: % passed; % failed;\r\n", passCounter, testCount - passCounter
    );
    gx::println(
        "Asserts: % count; % passed; % failed;\r\n",
        assert_counter,
        success_counter,
        failed_counter
    );
}
}
} // namespace gx
