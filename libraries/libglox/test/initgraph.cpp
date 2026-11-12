#ifndef USE_MODULES
#include "glox/initgraph.hpp"
#include "glox/macros.hpp"
#include <string_view>
#include <vector>
#else
import std;
import glox;
#endif
#include "doctest.h"

using namespace glox::initgraph;

namespace {

struct trace_policy
{
    std::vector<std::string_view> realized_nodes;
    std::vector<std::string_view> activated;
    std::vector<std::string_view> unreached;

    void on_realize_node(node* n)
    {
        if (n->display_name() != nullptr) {
            realized_nodes.emplace_back(n->display_name());
        }
    }

    void pre_activate(node* n)
    {
        if (n->display_name() != nullptr) {
            activated.emplace_back(n->display_name());
        }
    }

    void report_unreached(node* n)
    {
        if (n->display_name() != nullptr) {
            unreached.emplace_back(n->display_name());
        }
    }

    void on_realize_edge(edge*) { }

    void post_activate(node*) { }
};

static_assert(
    engine_policy<default_policy>,
    "default_policy must satisfy engine_policy"
);
static_assert(
    engine_policy<trace_policy>,
    "trace_policy must satisfy engine_policy"
);

} // namespace

TEST_CASE("initgraph topological execution order with require and entail")
{
    engine<trace_policy> eng;
    std::vector<int> trace;

    stage bootDone(&eng, "boot_done");

    task initHw(&eng, "init_hw", [&] { trace.push_back(1); });

    task initMem(&eng, "init_mem", require(&initHw), [&] {
        trace.push_back(2);
    });

    task initSched(
        &eng, "init_sched", require(&initMem), entail(&bootDone), [&] {
            trace.push_back(3);
        }
    );

    task startUser(&eng, "start_user", require(&bootDone), [&] {
        trace.push_back(4);
    });

    REQUIRE(eng.run());
    REQUIRE(trace == std::vector<int> { 1, 2, 3, 4 });
    REQUIRE(
        eng.policy.activated
        == std::vector<std::string_view> {
            "init_hw", "init_mem", "init_sched", "boot_done", "start_user" }
    );
    REQUIRE(eng.policy.unreached.empty());
}

TEST_CASE("initgraph diamond dependency graph runs shared prerequisite once")
{
    engine<trace_policy> eng;
    int baseRuns = 0;
    std::vector<std::string_view> order;

    task base(&eng, "base", [&] {
        ++baseRuns;
        order.emplace_back("base");
    });

    task left(&eng, "left", require(&base), [&] {
        order.emplace_back("left");
    });

    task right(&eng, "right", require(&base), [&] {
        order.emplace_back("right");
    });

    task join(&eng, "join", require(&left, &right), [&] {
        order.emplace_back("join");
    });

    REQUIRE(eng.run());
    REQUIRE(baseRuns == 1);
    REQUIRE(order.size() == 4);
    REQUIRE(order.front() == "base");
    REQUIRE(order.back() == "join");
}

TEST_CASE("initgraph goal-directed run executes only required subgraph")
{
    engine<trace_policy> eng;
    bool ranA = false;
    bool ranB = false;
    bool ranUnrelated = false;

    task a(&eng, "a", [&] { ranA = true; });
    task b(&eng, "b", require(&a), [&] { ranB = true; });
    task c(&eng, "c", require(&b), [&] { ranUnrelated = true; });
    task d(&eng, "d", [&] { ranUnrelated = true; });

    REQUIRE(eng.run(&b));
    REQUIRE(ranA);
    REQUIRE(ranB);
    REQUIRE_FALSE(ranUnrelated);
    REQUIRE(eng.policy.activated == std::vector<std::string_view> { "a", "b" });
    REQUIRE(eng.policy.unreached.empty());
}

TEST_CASE("initgraph incremental runs skip already completed nodes")
{
    engine<> eng;
    int countA = 0;
    int countB = 0;

    task a(&eng, "a", [&] { ++countA; });
    task b(&eng, "b", require(&a), [&] { ++countB; });

    REQUIRE(eng.run(&a));
    REQUIRE(countA == 1);
    REQUIRE(countB == 0);

    REQUIRE(eng.run());
    REQUIRE(countA == 1);
    REQUIRE(countB == 1);
}

TEST_CASE("initgraph cycle returns false and reports unreached nodes")
{
    engine<trace_policy> eng;
    bool ranA = false;
    bool ranB = false;

    task a(&eng, "cycle_a", [&] { ranA = true; });
    task b(&eng, "cycle_b", require(&a), [&] { ranB = true; });

    // Close the cycle: b -> a
    edge backEdge(&eng, &b, &a);

    REQUIRE_FALSE(eng.run());
    REQUIRE_FALSE(ranA);
    REQUIRE_FALSE(ranB);
    REQUIRE(
        eng.policy.unreached
        == std::vector<std::string_view> { "cycle_a", "cycle_b" }
    );
}

TEST_CASE("initgraph goal-directed run ignores cycle outside wanted subgraph")
{
    engine<trace_policy> eng;
    bool ranValid = false;

    task valid(&eng, "valid", [&] { ranValid = true; });

    task deadA(&eng, "dead_a", [] { });
    task deadB(&eng, "dead_b", require(&deadA), [] { });
    edge deadCycle(&eng, &deadB, &deadA);

    REQUIRE(eng.run(&valid));
    REQUIRE(ranValid);
    REQUIRE(eng.policy.unreached.empty());
}
