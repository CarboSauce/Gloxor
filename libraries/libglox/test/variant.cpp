#include "glox/variant.hpp"
#include "doctest.h"
#include "utils.hpp"

#include <cstdint>
#include <cstring>
#include <new>
#include <string>

using glox::variant;

// Objects currently alive, derived from the counters.
inline int alive()
{
    return alloc_tracker::ctor_counter + alloc_tracker::copy_ctor_counter
         + alloc_tracker::move_ctor_counter - alloc_tracker::dtor_counter;
}

using tracker = alloc_tracker;

struct Pod
{
    int a;
    char b;
};
struct Big
{
    char data[100];
};

template <class V, class T>
concept can_emplace_type = requires(V v) { v.template emplace<T>(); };

// ============================================================================
// static_assert tests
// ============================================================================

// --- helper variable templates ---------------------------------------------
// static_assert(glox::index_of_v<int, int> == 0);
// static_assert(glox::index_of_v<int, char, int, float> == 1);
// static_assert(glox::index_of_v<float, char, int, float> == 2);
// static_assert(glox::index_of_v<double, char, int, float> == sv::npos);
// static_assert(glox::index_of_v<int> == sv::npos);
//
// static_assert(glox::all_unique_v<int>);
// static_assert(glox::all_unique_v<int, char, float>);
// static_assert(!glox::variant<int,int>::all_unique_v);
// static_assert(!glox::all_unique_v<int, char, int>);

// static_assert(glox::max_sizeof_v<char> == 1);
// static_assert(glox::max_sizeof_v<char, double, short> == sizeof(double));
// static_assert(glox::max_sizeof_v<Big, int> == sizeof(Big));

// --- layout ----------------------------------------------------------------
static_assert(alignof(variant<char, double, short>) == alignof(double));
static_assert(alignof(variant<char, short>) == alignof(short));
static_assert(sizeof(variant<char>) == 2);
static_assert(sizeof(variant<int, double>) == 16);
static_assert(sizeof(variant<Big, int>) == sizeof(Big) + 4);
static_assert(
    sizeof(variant<unsigned char, signed char>) == sizeof(unsigned char) + 1
);

// --- triviality follows the alternatives -----------------------------------
using TrivialV = variant<int, float, char, Pod>;
static_assert(std::is_trivially_copyable_v<TrivialV>);
static_assert(std::is_trivially_destructible_v<TrivialV>);
static_assert(std::is_trivially_copy_constructible_v<TrivialV>);
static_assert(std::is_trivially_move_constructible_v<TrivialV>);
static_assert(std::is_trivially_copy_assignable_v<TrivialV>);
static_assert(std::is_trivially_move_assignable_v<TrivialV>);
static_assert(
    !std::is_trivially_default_constructible_v<TrivialV>
); // must set idx_ = 0

using tracker_v = variant<int, tracker>;
static_assert(!std::is_trivially_copyable_v<tracker_v>);
static_assert(!std::is_trivially_destructible_v<tracker_v>);
static_assert(!std::is_trivially_copy_constructible_v<tracker_v>);
static_assert(!std::is_trivially_move_constructible_v<tracker_v>);

using string_v = variant<int, std::string>;
static_assert(!std::is_trivially_copyable_v<string_v>);
static_assert(!std::is_trivially_destructible_v<string_v>);

// --- special members exist --------------------------------------------------
static_assert(std::is_default_constructible_v<tracker_v>);
static_assert(std::is_copy_constructible_v<tracker_v>);
static_assert(std::is_move_constructible_v<tracker_v>);
static_assert(std::is_copy_assignable_v<tracker_v>);
static_assert(std::is_move_assignable_v<tracker_v>);

// --- converting construction -----------------------------------------------
static_assert(std::is_constructible_v<variant<int, double>, int>);
static_assert(std::is_constructible_v<variant<int, double>, double>);
static_assert(std::is_constructible_v<variant<int, double>, const double&>);
static_assert(!std::is_constructible_v<variant<int, double>, std::string>);

// --- emplace constraints ----------------------------------------------------
static_assert(can_emplace_type<variant<int, double>, int>);
static_assert(can_emplace_type<variant<int, double>, double>);
static_assert(!can_emplace_type<variant<int, double>, std::string>);
static_assert(!can_emplace_type<variant<int, double>, char>);

// --- accessor return types --------------------------------------------------
static_assert(std::is_same_v<
    decltype(std::declval<variant<int, double>&>().get_if<int>()),
    int*
>);
static_assert(std::is_same_v<
    decltype(std::declval<const variant<int, double>&>().get_if<int>()),
    const int*
>);
static_assert(std::is_same_v<
    decltype(std::declval<variant<int, double>&>().get_if<1>()),
    double*
>);
static_assert(std::is_same_v<
    decltype(std::declval<variant<int, double>&>().emplace<double>(1.0)),
    double&
>);
static_assert(std::is_same_v<
    decltype(std::declval<variant<int, double>&>().emplace<0>(1)),
    int&
>);

// ============================================================================
// doctest runtime tests
// ============================================================================

TEST_CASE("default construction picks alternative 0")
{
    variant<int, double> a;
    CHECK(a.index() == 0);
    REQUIRE(a.get_if<int>() != nullptr);
    CHECK(a.get_if<double>() == nullptr);

    tracker::reset_counters();
    {
        variant<tracker, int> b;
        CHECK(b.index() == 0);
        CHECK(b.get_if<tracker>()->value == 0);
        CHECK(tracker::ctor_counter == 1);
        CHECK(tracker::copy_ctor_counter == 0);
        CHECK(tracker::move_ctor_counter == 0);
    }
    CHECK(alive() == 0);
}

TEST_CASE("converting construction selects the matching alternative")
{
    variant<int, double, char> a(42);
    CHECK(a.index() == 0);
    CHECK(*a.get_if<int>() == 42);

    variant<int, double, char> b(3.5);
    CHECK(b.index() == 1);
    CHECK(*b.get_if<double>() == doctest::Approx(3.5));

    variant<int, double, char> c('x');
    CHECK(c.index() == 2);
    CHECK(*c.get_if<char>() == 'x');

    variant<int, std::string> d(std::string("hello"));
    CHECK(d.index() == 1);
    CHECK(*d.get_if<std::string>() == "hello");
}

TEST_CASE(
    "converting construction from an lvalue tracker copies, from an rvalue "
    "moves"
)
{
    tracker::reset_counters();
    {
        tracker t(5);
        variant<int, tracker> a(t); // lvalue -> copy
        CHECK(tracker::copy_ctor_counter == 1);
        CHECK(tracker::move_ctor_counter == 0);
        CHECK(a.get_if<tracker>()->value == 5);

        variant<int, tracker> b(std::move(t)); // rvalue -> move
        CHECK(tracker::move_ctor_counter == 1);
        CHECK(b.get_if<tracker>()->value == 5);
    }
    CHECK(alive() == 0);
}

TEST_CASE("emplace by type and by index")
{
    variant<int, double, std::string> v;

    int& i = v.emplace<int>(7);
    CHECK(v.index() == 0);
    CHECK(&i == v.get_if<int>());
    CHECK(i == 7);

    double& d = v.emplace<1>(2.25);
    CHECK(v.index() == 1);
    CHECK(&d == v.get_if<1>());
    CHECK(d == doctest::Approx(2.25));

    std::string& s = v.emplace<std::string>(5, 'z');
    CHECK(v.index() == 2);
    CHECK(s == "zzzzz");
}

TEST_CASE("emplace constructs in place without copies or moves")
{
    tracker::reset_counters();
    {
        variant<int, tracker> v;
        v.emplace<tracker>(9);
        CHECK(tracker::ctor_counter == 1);
        CHECK(tracker::copy_ctor_counter == 0);
        CHECK(tracker::move_ctor_counter == 0);
        CHECK(v.get_if<tracker>()->value == 9);
    }
    CHECK(alive() == 0);
}

TEST_CASE("emplace returns a reference into the variant")
{
    variant<int, double> v;
    int& r = v.emplace<int>(1);
    r = 99;
    CHECK(*v.get_if<int>() == 99);
}

TEST_CASE("get_if returns nullptr for the wrong alternative")
{
    variant<int, double, char> v(1.5);
    CHECK(v.get_if<int>() == nullptr);
    CHECK(v.get_if<0>() == nullptr);
    CHECK(v.get_if<char>() == nullptr);
    CHECK(v.get_if<double>() != nullptr);
    CHECK(v.get_if<1>() != nullptr);
}

TEST_CASE("get_if on a const variant")
{
    const variant<int, double> v(5);
    const int* p = v.get_if<int>();
    REQUIRE(p != nullptr);
    CHECK(*p == 5);
    CHECK(v.get_if<double>() == nullptr);
}

TEST_CASE("visit dispatches on the active alternative")
{
    auto f = [](auto& x) -> int {
        using T = std::remove_cvref_t<decltype(x)>;
        if constexpr (std::is_same_v<T, int>)
            return x;
        else if constexpr (std::is_same_v<T, double>)
            return static_cast<int>(x * 2);
        else
            return -1;
    };

    variant<int, double, char> v(10);
    CHECK(v.visit(f) == 10);
    v = 4.0;
    CHECK(v.visit(f) == 8);
    v = 'c';
    CHECK(v.visit(f) == -1);

    const variant<int, double, char> cv(3);
    CHECK(cv.visit(f) == 3);
}

TEST_CASE("visit can mutate the alternative in place")
{
    variant<int, double> v(5);
    v.visit([](auto& x) { x = x + 1; });
    CHECK(*v.get_if<int>() == 6);

    tracker::reset_counters();
    {
        variant<int, tracker> t;
        t.emplace<tracker>(1);
        t.visit([](auto& x) {
            if constexpr (
                std::is_same_v<std::remove_cvref_t<decltype(x)>, tracker>
            )
                x.value = 100;
        });
        CHECK(t.get_if<tracker>()->value == 100);
    }
    CHECK(alive() == 0);
}

TEST_CASE("visit reaches every entry of the jump table")
{
    auto idx_of = [](auto& x) -> int {
        using T = std::remove_cvref_t<decltype(x)>;
        if constexpr (std::is_same_v<T, char>)
            return 0;
        else if constexpr (std::is_same_v<T, short>)
            return 1;
        else if constexpr (std::is_same_v<T, int>)
            return 2;
        else if constexpr (std::is_same_v<T, long>)
            return 3;
        else if constexpr (std::is_same_v<T, float>)
            return 4;
        else
            return 5;
    };
    variant<char, short, int, long, float, double> v;
    v.emplace<char>();
    CHECK(v.visit(idx_of) == 0);
    v.emplace<short>();
    CHECK(v.visit(idx_of) == 1);
    v.emplace<int>();
    CHECK(v.visit(idx_of) == 2);
    v.emplace<long>();
    CHECK(v.visit(idx_of) == 3);
    v.emplace<float>();
    CHECK(v.visit(idx_of) == 4);
    v.emplace<double>();
    CHECK(v.visit(idx_of) == 5);
}

TEST_CASE("lifetime: destructor runs on scope exit")
{
    tracker::reset_counters();
    {
        variant<int, tracker> v;
        v.emplace<tracker>(1);
        CHECK(alive() == 1);
        CHECK(tracker::dtor_counter == 0);
    }
    CHECK(tracker::dtor_counter == 1);
    CHECK(alive() == 0);
}

TEST_CASE("lifetime: default-constructed alternative 0 is destroyed")
{
    tracker::reset_counters();
    {
        variant<tracker, int> v;
        CHECK(alive() == 1);
    }
    CHECK(tracker::dtor_counter == 1);
    CHECK(alive() == 0);
}

TEST_CASE("lifetime: a trivial active alternative destroys nothing")
{
    tracker::reset_counters();
    {
        variant<int, tracker> v(3);
        CHECK(tracker::ctor_counter == 0);
    }
    CHECK(tracker::dtor_counter == 0);
}

TEST_CASE("lifetime: emplace destroys the previous value")
{
    tracker::reset_counters();
    variant<tracker, int> v; // 1 alive
    CHECK(alive() == 1);

    v.emplace<int>(5); // tracker destroyed
    CHECK(alive() == 0);
    CHECK(tracker::dtor_counter == 1);

    v.emplace<tracker>(9);
    CHECK(alive() == 1);

    v.emplace<tracker>(10); // old destroyed, new constructed
    CHECK(alive() == 1);
    CHECK(tracker::dtor_counter == 2);
    CHECK(v.get_if<tracker>()->value == 10);

    // emplace never uses element assignment
    CHECK(tracker::copy_assignment_counter == 0);
    CHECK(tracker::move_assignment_counter == 0);
}

TEST_CASE("copy construction copies the active alternative")
{
    tracker::reset_counters();
    {
        variant<int, tracker> a;
        a.emplace<tracker>(42);

        variant<int, tracker> b(a);
        CHECK(tracker::copy_ctor_counter == 1);
        CHECK(tracker::move_ctor_counter == 0);
        CHECK(alive() == 2);
        CHECK(b.index() == 1);
        CHECK(b.get_if<tracker>()->value == 42);
        CHECK(a.get_if<tracker>()->value == 42);
    }
    CHECK(alive() == 0);
}

TEST_CASE("copy construction of a trivial active alternative copies no tracker")
{
    tracker::reset_counters();
    variant<int, tracker> a(7);
    variant<int, tracker> b(a);
    CHECK(b.index() == 0);
    CHECK(*b.get_if<int>() == 7);
    CHECK(tracker::copy_ctor_counter == 0);
    CHECK(tracker::ctor_counter == 0);
}

TEST_CASE("move construction moves the active alternative")
{
    tracker::reset_counters();
    {
        variant<int, tracker> a;
        a.emplace<tracker>(42);

        variant<int, tracker> b(std::move(a));
        CHECK(tracker::move_ctor_counter == 1);
        CHECK(tracker::copy_ctor_counter == 0);
        CHECK(b.index() == 1);
        CHECK(b.get_if<tracker>()->value == 42);
        CHECK(a.index() == 1); // moved-from variant still holds a tracker
        CHECK(alive() == 2);
    }
    CHECK(alive() == 0);
}

TEST_CASE("copy construction with a std::string alternative")
{
    variant<int, std::string> a(
        std::string("a fairly long string that avoids SSO.......")
    );
    variant<int, std::string> b(a);
    CHECK(*b.get_if<std::string>() == *a.get_if<std::string>());
    b.get_if<std::string>()->push_back('!');
    CHECK(a.get_if<std::string>()->back() == '.'); // deep copy
}

TEST_CASE("copy assignment across different alternatives")
{
    tracker::reset_counters();
    {
        variant<int, tracker> a; // int
        variant<int, tracker> b;
        b.emplace<tracker>(7);

        a = b; // int -> tracker
        CHECK(a.index() == 1);
        CHECK(a.get_if<tracker>()->value == 7);
        CHECK(tracker::copy_ctor_counter == 1);
        CHECK(alive() == 2);

        b.emplace<int>(3); // destroys b's tracker
        CHECK(alive() == 1);

        a = b; // tracker -> int, tracker destroyed
        CHECK(a.index() == 0);
        CHECK(*a.get_if<int>() == 3);
        CHECK(alive() == 0);
    }
    CHECK(alive() == 0);
}

TEST_CASE("copy assignment within the same alternative reconstructs")
{
    tracker::reset_counters();
    {
        variant<int, tracker> a;
        variant<int, tracker> b;
        a.emplace<tracker>(1);
        b.emplace<tracker>(2);

        const int dtors_before = tracker::dtor_counter;
        a = b;
        CHECK(a.get_if<tracker>()->value == 2);
        CHECK(tracker::dtor_counter == dtors_before + 1); // old value destroyed
        CHECK(tracker::copy_ctor_counter == 1); // new value copy-constructed
        CHECK(tracker::copy_assignment_counter == 0); // no element assignment
        CHECK(alive() == 2);
    }
    CHECK(alive() == 0);
}

TEST_CASE("copy assignment with a std::string alternative")
{
    variant<int, std::string> a(std::string("one"));
    variant<int, std::string> b(std::string("two"));
    a = b;
    CHECK(*a.get_if<std::string>() == "two");
    CHECK(*b.get_if<std::string>() == "two");
}

TEST_CASE("move assignment")
{
    tracker::reset_counters();
    {
        variant<int, tracker> a;
        variant<int, tracker> b;
        b.emplace<tracker>(11);

        a = std::move(b);
        CHECK(a.index() == 1);
        CHECK(a.get_if<tracker>()->value == 11);
        CHECK(tracker::move_ctor_counter == 1);
        CHECK(tracker::move_assignment_counter == 0);
        CHECK(tracker::copy_ctor_counter == 0);
    }
    CHECK(alive() == 0);
}

TEST_CASE("self assignment is a no-op")
{
    tracker::reset_counters();
    {
        variant<int, tracker> v;
        v.emplace<tracker>(5);
        auto& alias = v; // avoids -Wself-assign-overloaded
        const int dtors_before = tracker::dtor_counter;

        v = alias;
        CHECK(v.get_if<tracker>()->value == 5);
        CHECK(tracker::dtor_counter == dtors_before);
        CHECK(alive() == 1);

        v = std::move(alias);
        CHECK(v.get_if<tracker>()->value == 5);
        CHECK(tracker::dtor_counter == dtors_before);
        CHECK(alive() == 1);
    }
    CHECK(alive() == 0);
}

TEST_CASE("assigning a value through the converting constructor")
{
    variant<int, double> v;
    v = 2.5;
    CHECK(v.index() == 1);
    CHECK(*v.get_if<double>() == doctest::Approx(2.5));
    v = 8;
    CHECK(v.index() == 0);
    CHECK(*v.get_if<int>() == 8);

    tracker::reset_counters();
    {
        variant<int, tracker> t;
        t = tracker(4); // temp -> converting ctor (move) -> move assign
        CHECK(t.index() == 1);
        CHECK(t.get_if<tracker>()->value == 4);
    }
    CHECK(alive() == 0);
}

TEST_CASE("trivial variants can be memcpy'd")
{
    variant<int, float, Pod> src;
    src.emplace<Pod>(Pod { 7, 'q' });

    alignas(variant<int, float, Pod>) unsigned char raw[sizeof(src)];
    std::memcpy(raw, &src, sizeof(src));
    auto& dst = *std::launder(reinterpret_cast<variant<int, float, Pod>*>(raw));

    CHECK(dst.index() == 2);
    REQUIRE(dst.get_if<Pod>() != nullptr);
    CHECK(dst.get_if<Pod>()->a == 7);
    CHECK(dst.get_if<Pod>()->b == 'q');
}

TEST_CASE("trivial variants copy by value")
{
    variant<int, double> a(3.25);
    variant<int, double> b = a;
    variant<int, double> c;
    c = b;
    CHECK(c.index() == 1);
    CHECK(*c.get_if<double>() == doctest::Approx(3.25));
}

TEST_CASE("a large alternative survives copy")
{
    variant<int, Big> v;
    Big& big = v.emplace<Big>();
    for (std::size_t i = 0; i < sizeof(big.data); ++i)
        big.data[i] = static_cast<char>(i);

    variant<int, Big> copy(v);
    CHECK(copy.index() == 1);
    CHECK(
        std::memcmp(copy.get_if<Big>()->data, big.data, sizeof(big.data)) == 0
    );
}

TEST_CASE("alignment of the active alternative is respected")
{
    variant<char, double> v;
    v.emplace<double>(1.0);
    CHECK(
        reinterpret_cast<std::uintptr_t>(v.get_if<double>()) % alignof(double)
        == 0
    );
}

TEST_CASE("overload test")
{
    variant<char, double> v;
    v.emplace<double>(1.0);
    auto res = v.visit(
        glox::overload {
            [](double c) { return c + 2; },
            [](char c) { return c + 1; },
        }
    );
    CHECK(res == 3);
}

TEST_CASE("repeated emplace cycling through all alternatives")
{
    tracker::reset_counters();
    {
        variant<int, tracker, std::string, double> v;
        for (int round = 0; round < 100; ++round) {
            v.emplace<tracker>(round);
            CHECK(v.get_if<tracker>()->value == round);
            CHECK(alive() == 1);
            v.emplace<std::string>("round");
            CHECK(alive() == 0);
            v.emplace<double>(round * 0.5);
            v.emplace<int>(round);
        }
        CHECK(tracker::ctor_counter == 100);
        CHECK(tracker::dtor_counter == 100);
    }
    CHECK(alive() == 0);
}
