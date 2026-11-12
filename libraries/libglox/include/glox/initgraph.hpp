#pragma once

#ifndef USE_MODULES
#include "array.hpp"
#include "assert.hpp"
#include "detail/moveutils.hpp"
#include "intrusive_list.hpp"
#include "macros.hpp"
#include "metaprog.hpp"
#include <cstddef>
#include <type_traits>
#endif

// This is port of initgraph from Managarm under MIT license

namespace glox::initgraph {
GLOX_BEGIN_EXPORT

struct node;
struct edge;

struct default_policy
{
    static constexpr void on_realize_node(node*) noexcept { }
    static constexpr void on_realize_edge(edge*) noexcept { }
    static constexpr void pre_activate(node*) noexcept { }
    static constexpr void post_activate(node*) noexcept { }
    static constexpr void report_unreached(node*) noexcept { }
};

template <typename P>
concept engine_policy = requires(P p, edge* e, node* n) {
    { p.on_realize_node(n) };
    { p.on_realize_edge(e) };
    { p.pre_activate(n) };
    { p.post_activate(n) };
    { p.report_unreached(n) };
};

enum class node_type
{
    none,
    stage,
    task
};

struct edge
{
    friend struct node;

    template <typename Engine>
    edge(Engine* eng, node* src, node* dst)
        : source_(src)
        , target_(dst)
    {
        eng->realize_edge(this);
    }

    edge(const edge&) = delete;
    edge& operator=(const edge&) = delete;

    node* source() const noexcept
    {
        return source_;
    }
    node* target() const noexcept
    {
        return target_;
    }

    node* source_;
    node* target_;
    list_node out_hook_;
    list_node in_hook_;
};

struct node
{
    using activate_fn = void (*)(void*);

    template <typename Engine>
    node(
        node_type type,
        Engine* eng,
        const char* name = nullptr,
        activate_fn fn = nullptr,
        void* ctx = nullptr
    )
        : type_(type)
        , display_name_(name)
        , fn_(fn)
        , ctx_(ctx)
    {
        eng->realize_node(this);
    }

    node(const node&) = delete;
    node& operator=(const node&) = delete;

    node_type type() const noexcept
    {
        return type_;
    }
    const char* display_name() const noexcept
    {
        return display_name_;
    }

    void activate()
    {
        if (fn_ != nullptr) {
            fn_(ctx_);
        }
    }

    node_type type_;
    const char* display_name_;
    activate_fn fn_;
    void* ctx_;

    intrusive_list<edge, &edge::out_hook_> out_list_;
    intrusive_list<edge, &edge::in_hook_> in_list_;
    list_node nodes_hook_;
    list_node queue_hook_;

    bool done_ = false;
    bool wanted_ = false;
    unsigned int unsatisfied_ = 0;
};

template <engine_policy Policy = default_policy>
struct engine
{
    [[no_unique_address]] Policy policy { };

    engine() = default;
    explicit engine(Policy p)
        : policy(RVALUE(p))
    {
    }

    engine(const engine&) = delete;
    engine& operator=(const engine&) = delete;

    void realize_node(node* n)
    {
        nodes_.push_back(n);
        policy.on_realize_node(n);
    }

    void realize_edge(edge* e)
    {
        e->source_->out_list_.push_back(e);
        e->target_->in_list_.push_back(e);
        ++e->target_->unsatisfied_;
        policy.on_realize_edge(e);
    }

    [[nodiscard]] bool run(node* goal = nullptr)
    {
        intrusive_list<node, &node::queue_hook_> q;

        if (goal != nullptr) {
            if (!goal->wanted_) {
                q.push_back(goal);
                goal->wanted_ = true;
            }
            while (!q.is_empty()) {
                node* curr = &q.front();
                q.erase(q.begin());

                for (auto& e : curr->in_list_) {
                    if (!e.source_->wanted_) {
                        q.push_back(e.source_);
                        e.source_->wanted_ = true;
                    }
                }
            }
        } else {
            for (auto& n : nodes_) {
                n.wanted_ = true;
            }
        }

        for (auto& n : nodes_) {
            if (n.wanted_ && !n.done_ && n.unsatisfied_ == 0) {
                pending_.push_back(&n);
            }
        }

        while (!pending_.is_empty()) {
            node* curr = &pending_.front();
            pending_.erase(pending_.begin());

            GLOX_ASSERT(curr->wanted_ && !curr->done_);

            policy.pre_activate(curr);
            curr->activate();
            curr->done_ = true;
            policy.post_activate(curr);

            for (auto& e : curr->out_list_) {
                node* succ = e.target_;
                GLOX_ASSERT(succ->unsatisfied_ > 0);
                if (--succ->unsatisfied_ == 0 && succ->wanted_
                    && !succ->done_) {
                    pending_.push_back(succ);
                }
            }
        }

        bool allReached = true;
        for (auto& n : nodes_) {
            if (n.wanted_ && !n.done_) {
                policy.report_unreached(&n);
                allReached = false;
            }
        }

        return allReached;
    }

private:
    intrusive_list<node, &node::nodes_hook_> nodes_;
    intrusive_list<node, &node::queue_hook_> pending_;
};

struct stage final : node
{
    template <typename Engine>
    stage(Engine* eng, const char* name = nullptr)
        : node(node_type::stage, eng, name)
    {
    }
};

template <std::size_t N>
struct require
{
    template <typename... Args>
        requires(std::is_convertible_v<Args, node*> && ...)
    require(Args&&... args)
        : nodes { { args... } }
    {
    }

    glox::array<node*, N> nodes;
};

template <typename... Args>
require(Args&&...) -> require<sizeof...(Args)>;

template <std::size_t N>
struct entail
{
    template <typename... Args>
        requires(std::is_convertible_v<Args, node*> && ...)
    entail(Args&&... args)
        : nodes { { args... } }
    {
    }

    glox::array<node*, N> nodes;
};

template <typename... Args>
entail(Args&&...) -> entail<sizeof...(Args)>;

template <typename F, std::size_t NR = 0, std::size_t NE = 0>
struct task final : node
{
    template <typename Engine>
    task(
        Engine* eng,
        const char* name,
        require<NR> r,
        entail<NE> e,
        F invocable
    )
        : node(node_type::task, eng, name, &invoke_thunk, &invocable_)
        , invocable_(RVALUE(invocable))
        , r_edges_(
              make_edges_to(eng, r.nodes, glox::make_index_sequence<NR> { })
          )
        , e_edges_(
              make_edges_from(eng, e.nodes, glox::make_index_sequence<NE> { })
          )
    {
    }

    template <typename Engine>
    task(Engine* eng, const char* name, F invocable)
        : task(eng, name, { }, { }, RVALUE(invocable))
    {
    }

    template <typename Engine>
    task(Engine* eng, const char* name, require<NR> r, F invocable)
        : task(eng, name, r, { }, RVALUE(invocable))
    {
    }

    template <typename Engine>
    task(Engine* eng, const char* name, entail<NE> e, F invocable)
        : task(eng, name, { }, e, RVALUE(invocable))
    {
    }

private:
    static void invoke_thunk(void* ctx)
    {
        (*static_cast<F*>(ctx))();
    }

    template <typename Engine, std::size_t... Is>
    glox::array<edge, NR> make_edges_to(
        Engine* eng,
        const glox::array<node*, NR>& arr,
        glox::index_sequence<Is...>
    )
    {
        return { { { eng, arr[Is], this }... } };
    }

    template <typename Engine, std::size_t... Is>
    glox::array<edge, NE> make_edges_from(
        Engine* eng,
        const glox::array<node*, NE>& arr,
        glox::index_sequence<Is...>
    )
    {
        return { { { eng, this, arr[Is] }... } };
    }

    F invocable_;
    glox::array<edge, NR> r_edges_;
    glox::array<edge, NE> e_edges_;
};

template <typename Engine, typename F>
task(Engine*, const char*, F) -> task<F, 0, 0>;

template <typename Engine, typename F, std::size_t NR>
task(Engine*, const char*, require<NR>, F) -> task<F, NR, 0>;

template <typename Engine, typename F, std::size_t NE>
task(Engine*, const char*, entail<NE>, F) -> task<F, 0, NE>;

template <typename Engine, typename F, std::size_t NR, std::size_t NE>
task(Engine*, const char*, require<NR>, entail<NE>, F) -> task<F, NR, NE>;

GLOX_END_EXPORT
} // namespace glox::initgraph
