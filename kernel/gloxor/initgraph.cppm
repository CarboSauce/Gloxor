export module gloxor.initgraph;

import gloxor.system;
export import glox;

export namespace gx {
struct DefaultPolicy
{
    void on_realize_node(glox::initgraph::node* n)
    {
        if (n->display_name() != nullptr) {
            gx::println("Realizing %", n->display_name());
        }
    }

    void pre_activate(glox::initgraph::node* n)
    {
        if (n->display_name() != nullptr) {
            gx::println("Activated %", n->display_name());
        }
    }

    void report_unreached(glox::initgraph::node* n)
    {
        if (n->display_name() != nullptr) {
            gx::println("Unreached %", n->display_name());
        }
    }

    void on_realize_edge(glox::initgraph::edge*) { }

    void post_activate(glox::initgraph::node*) { }
};

static_assert(glox::initgraph::engine_policy<DefaultPolicy>);

constinit inline glox::initgraph::engine<DefaultPolicy> postCpuInit { };
constinit inline glox::initgraph::engine<DefaultPolicy> preCpuInit { };
constinit inline glox::initgraph::engine<DefaultPolicy> preSchedInit { };
constinit inline glox::initgraph::engine<DefaultPolicy> postSchedInit { };
} // namespace gx
