/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include <cppwamp/tokentrienode.hpp>
#include <utility>
#include <vector>
#include <catch2/catch.hpp>
#include "valuemodels.hpp"

using namespace wamp;
using namespace wamp::internal;

namespace
{

//------------------------------------------------------------------------------
template <typename X, typename T>
void checkOptionalValue(const X& x, const T& value)
{
    REQUIRE(x.has_value());
    CHECK(bool(x));
    CHECK(x.value() == value);
    CHECK(*x == value);
    CHECK(x == X{value});
}

//------------------------------------------------------------------------------
template <typename X>
void checkNullOptionalValue(const X& x)
{
    REQUIRE_FALSE(x.has_value());
    CHECK_FALSE(bool(x));
    CHECK_THROWS(x.value());
}

using Small = wamp::test::SmallValue;
using Large = wamp::test::LargeValue<2*sizeof(std::string)>;

//------------------------------------------------------------------------------
template <typename X, typename... Ts>
using optional_emplace_fn =
    decltype(std::declval<X&>().emplace(std::declval<Ts>()...));

//------------------------------------------------------------------------------
template <typename X>
using optional_swap_fn = decltype(std::declval<X&>().swap(std::declval<X&>()));

//------------------------------------------------------------------------------
template <typename T>
using opt = TokenTrieOptionalValue<T>;

} // anonymous namespace

//------------------------------------------------------------------------------
TEMPLATE_TEST_CASE("TokenTrieOptionalValue value construction", "[Uri]",
                   Small, Large)
{
    SECTION("default construction")
    {
        opt<TestType> x;
        checkNullOptionalValue(x);
    }

    SECTION("copy value")
    {
        const TestType n{42};
        opt<TestType> x{n};
        checkOptionalValue(x, 42);
        CHECK(n.value == 42);
        CHECK(x.value().copyConstructed);
    }

    SECTION("move value")
    {
        TestType n{42};
        opt<TestType> x{std::move(n)};
        checkOptionalValue(x, 42);
        CHECK(n.value == 0);
        CHECK(x.value().moveConstructed);
    }

    SECTION("in-place")
    {
        using Pair = std::pair<TestType, TestType>;
        TestType a = 1;
        TestType b = 2;
        Pair p(a, b);
        opt<Pair> x{in_place, std::move(a), b.value};
        checkOptionalValue(x, p);
        CHECK(x.value().first.moveConstructed);
        CHECK(x.value().second.valueConstructed);
        CHECK(a.value == 0);
        CHECK(b.value == 2);
    }

    SECTION("in-place default construction")
    {
        opt<TestType> x{in_place};
        checkOptionalValue(x, 0);
        CHECK(x.value().defaultConstructed);
    }

    SECTION("in-place initializer list")
    {
        using Vec = std::vector<TestType>;
        TestType a = 1;
        TestType b = 2;
        TestType c = 3;
        opt<Vec> x{in_place, {a, b, c}};
        checkOptionalValue(x, Vec{a, b, c});
    }

    SECTION("copy optional with value")
    {
        const opt<TestType> x{42};
        opt<TestType> y{x};
        checkOptionalValue(y, 42);
        CHECK(x.value() == 42);
        CHECK(y.value().copyConstructed);
    }

    SECTION("move optional with value")
    {
        opt<TestType> x{42};
        opt<TestType> y{std::move(x)};
        checkOptionalValue(y, 42);
        REQUIRE(x.has_value());
        CHECK(x.value().value == 0);
        CHECK(y.value().moveConstructed);
    }
}

//------------------------------------------------------------------------------
TEMPLATE_TEST_CASE("empty TokenTrieOptionalValue construction", "[Uri]",
                   Small, Large)
{
    SECTION("copy empty optional")
    {
        const opt<TestType> x;
        opt<TestType> y{x};
        checkNullOptionalValue(x);
        checkNullOptionalValue(y);
    }

    SECTION("move empty optional")
    {
        opt<TestType> x;
        opt<TestType> y{std::move(x)};
        checkNullOptionalValue(x);
        checkNullOptionalValue(y);
    }
}

//------------------------------------------------------------------------------
TEMPLATE_TEST_CASE("TokenTrieOptionalValue copy-assign value", "[Uri]",
                   Small, Large)
{
    SECTION("lhs has value")
    {
        opt<TestType> lhs{1};
        TestType rhs{42};

        SECTION("no exceptions")
        {
            lhs.value().resetFlags();
            lhs = rhs;
            checkOptionalValue(lhs, 42);
            CHECK(lhs.value().copyAssigned);
            CHECK(rhs.value == 42);
        }

        SECTION("assignment throws")
        {
            rhs.poison();
            CHECK_THROWS_AS(lhs = rhs, std::bad_alloc);
            checkOptionalValue(lhs, 1);
            CHECK(rhs.value == 42);
        }
    }

    SECTION("lhs is empty")
    {
        opt<TestType> lhs;
        TestType rhs{42};

        SECTION("no exceptions")
        {
            lhs = rhs;
            checkOptionalValue(lhs, 42);
            CHECK(lhs.value().copyConstructed);
            CHECK(rhs.value == 42);
        }

        SECTION("assignment throws")
        {
            rhs.poison();
            CHECK_THROWS_AS(lhs = rhs, std::bad_alloc);
            checkNullOptionalValue(lhs);
            CHECK(rhs.value == 42);
        }
    }
}

//------------------------------------------------------------------------------
TEMPLATE_TEST_CASE("TokenTrieOptionalValue move-assign value", "[Uri]",
                   Small, Large)
{
    SECTION("lhs has value")
    {
        opt<TestType> lhs{1};
        lhs.value().resetFlags();
        TestType rhs{42};

        lhs = std::move(rhs);
        checkOptionalValue(lhs, 42);
        CHECK(lhs.value().moveAssigned);
        CHECK(rhs.value == 0);
    }

    SECTION("lhs is empty")
    {
        opt<TestType> lhs;
        TestType rhs{42};

        lhs = std::move(rhs);
        checkOptionalValue(lhs, 42);
        CHECK(lhs.value().moveConstructed);
        CHECK(rhs.value == 0);
    }
}

//------------------------------------------------------------------------------
TEMPLATE_TEST_CASE("TokenTrieOptionalValue copy assignment", "[Uri]",
                   Small, Large)
{
    SECTION("lhs has value, rhs has value")
    {
        opt<TestType> lhs{1};
        opt<TestType> rhs{42};

        SECTION("no exceptions")
        {
            lhs.value().resetFlags();
            lhs = rhs;
            checkOptionalValue(lhs, 42);
            checkOptionalValue(rhs, 42);
            CHECK(lhs.value().copyAssigned);
        }

        SECTION("assignment throws")
        {
            rhs.value().poison();
            CHECK_THROWS_AS(lhs = rhs, std::bad_alloc);
            checkOptionalValue(lhs, 1);
            checkOptionalValue(rhs, 42);
        }
    }

    SECTION("lhs has value, rhs is empty")
    {
        opt<TestType> lhs{42};
        opt<TestType> rhs;
        lhs = rhs;
        checkNullOptionalValue(lhs);
        checkNullOptionalValue(rhs);
    }

    SECTION("lhs is empty, rhs has value")
    {
        opt<TestType> lhs;
        opt<TestType> rhs{42};

        SECTION("no exceptions")
        {
            lhs = rhs;
            checkOptionalValue(lhs, 42);
            checkOptionalValue(rhs, 42);
            CHECK(lhs.value().copyConstructed);
        }

        SECTION("constructing temporary throws")
        {
            rhs.value().poison();
            CHECK_THROWS_AS(lhs = rhs, std::bad_alloc);
            checkNullOptionalValue(lhs);
            checkOptionalValue(rhs, 42);
        }
    }

    SECTION("lhs is empty, rhs is empty")
    {
        opt<TestType> lhs;
        opt<TestType> rhs;
        lhs = rhs;
        checkNullOptionalValue(lhs);
        checkNullOptionalValue(rhs);
    }
}

//------------------------------------------------------------------------------
TEMPLATE_TEST_CASE("TokenTrieOptionalValue move assignment", "[Uri]",
                   Small, Large)
{
    SECTION("lhs has value, rhs has value")
    {
        opt<TestType> lhs{1};
        opt<TestType> rhs{42};

        SECTION("no exceptions")
        {
            lhs.value().resetFlags();
            lhs = std::move(rhs);
            checkOptionalValue(lhs, 42);
            checkOptionalValue(rhs, 0);
            CHECK(lhs.value().moveAssigned);
        }

        SECTION("assignment throws")
        {
            rhs.value().poison();
            CHECK_THROWS_AS(lhs = std::move(rhs), std::bad_alloc);
            checkOptionalValue(lhs, 1);
            checkOptionalValue(rhs, 42);
        }
    }

    SECTION("lhs has value, rhs is empty")
    {
        opt<TestType> lhs{42};
        opt<TestType> rhs;

        lhs = std::move(rhs);
        checkNullOptionalValue(lhs);
        checkNullOptionalValue(rhs);
    }

    SECTION("lhs is empty, rhs has value")
    {
        opt<TestType> lhs;
        opt<TestType> rhs{42};

        SECTION("no exceptions")
        {
            lhs = std::move(rhs);
            checkOptionalValue(lhs, 42);
            checkOptionalValue(rhs, 0);
            CHECK(lhs.value().moveConstructed);
        }

        SECTION("assignment throws")
        {
            rhs.value().poison();
            CHECK_THROWS_AS(lhs = std::move(rhs), std::bad_alloc);
            checkNullOptionalValue(lhs);
            checkOptionalValue(rhs, 42);
        }
    }

    SECTION("lhs is empty, rhs is empty")
    {
        opt<TestType> lhs;
        opt<TestType> rhs;
        lhs = std::move(rhs);
        checkNullOptionalValue(lhs);
        checkNullOptionalValue(rhs);
    }
}

//------------------------------------------------------------------------------
TEMPLATE_TEST_CASE("TokenTrieOptionalValue emplace", "[Uri]", Small, Large)
{
    using Pair = std::pair<TestType, TestType>;
    TestType a{1};
    TestType b{2};
    Pair p{a, b};

    SECTION("lhs has value")
    {
        opt<Pair> lhs{in_place, 0, 0};

        SECTION("no exceptions")
        {
            lhs.value().first.resetFlags();
            lhs.value().second.resetFlags();
            CHECK(lhs.emplace(a, std::move(b)) == p);
            checkOptionalValue(lhs, p);
            CHECK(a.value == 1);
            CHECK(b.value == 0);
            CHECK(lhs.value().first.copyConstructed);
            CHECK(lhs.value().second.moveConstructed);
        }

        SECTION("throws")
        {
            b.poison();
            CHECK_THROWS_AS(lhs.emplace(std::move(a), b), std::bad_alloc);
            checkNullOptionalValue(lhs);
            CHECK(a.value == 0);
            CHECK(b.value == 2);
        }
    }

    SECTION("lhs is empty")
    {
        opt<Pair> lhs;

        SECTION("no exceptions")
        {
            CHECK(lhs.emplace(a, std::move(b)) == p);
            checkOptionalValue(lhs, p);
            CHECK(a.value == 1);
            CHECK(b.value == 0);
            CHECK(lhs.value().first.copyConstructed);
            CHECK(lhs.value().second.moveConstructed);
        }

        SECTION("throws")
        {
            b.poison();
            CHECK_THROWS_AS(lhs.emplace(std::move(a), b), std::bad_alloc);
            checkNullOptionalValue(lhs);
            CHECK(a.value == 0);
            CHECK(b.value == 2);
        }
    }
}

//------------------------------------------------------------------------------
TEMPLATE_TEST_CASE("TokenTrieOptionalValue emplace initializer list", "[Uri]",
                   Small, Large)
{
    using Vec = std::vector<TestType>;
    TestType a{1};
    TestType b{2};
    TestType c{3};

    SECTION("lhs has value")
    {
        opt<Vec> lhs{in_place};

        SECTION("no exceptions")
        {
            CHECK(lhs.emplace({a, b, c}) == Vec{a, b, c});
            REQUIRE(lhs.has_value());
            CHECK(lhs.value() == Vec{a, b, c});
        }

        SECTION("throws")
        {
            b.poison();
            CHECK_THROWS_AS(lhs.emplace({a, b, c}), std::bad_alloc);
            checkOptionalValue(lhs, Vec{});
        }
    }

    SECTION("lhs is empty")
    {
        opt<Vec> lhs;

        SECTION("no exceptions")
        {
            CHECK(lhs.emplace({a, b, c}) == Vec{a, b, c});
            REQUIRE(lhs.has_value());
            CHECK(lhs.value() == Vec{a, b, c});
        }

        SECTION("throws")
        {
            b.poison();
            CHECK_THROWS_AS(lhs.emplace({a, b, c}), std::bad_alloc);
            checkNullOptionalValue(lhs);
        }
    }
}

//------------------------------------------------------------------------------
TEMPLATE_TEST_CASE("TokenTrieOptionalValue swap", "[Uri]", Small, Large)
{
    SECTION("value <-> value")
    {
        opt<TestType> a{1};
        opt<TestType> b{2};
        a.swap(b);
        checkOptionalValue(a, 2);
        checkOptionalValue(b, 1);
        CHECK(a.value().moveAssigned);
        CHECK(b.value().moveAssigned);

        a.value().resetFlags();
        b.value().resetFlags();
        swap(a, b);
        checkOptionalValue(a, 1);
        checkOptionalValue(b, 2);
        CHECK(a.value().moveAssigned);
        CHECK(b.value().moveAssigned);
    }

    SECTION("null <-> null")
    {
        opt<TestType> a;
        opt<TestType> b;
        a.swap(b);
        checkNullOptionalValue(a);
        checkNullOptionalValue(b);

        swap(a, b);
        checkNullOptionalValue(a);
        checkNullOptionalValue(b);
    }

    SECTION("value <-> null")
    {
        opt<TestType> a{1};
        opt<TestType> b;
        a.swap(b);
        checkNullOptionalValue(a);
        checkOptionalValue(b, 1);
        CHECK(b.value().moveConstructed);

        swap(a, b);
        checkOptionalValue(a, 1);
        checkNullOptionalValue(b);
        CHECK(a.value().moveConstructed);
    }
}

//------------------------------------------------------------------------------
TEST_CASE("TokenTrieOptionalValue comparisons", "[Uri]")
{
    SECTION("value <-> value")
    {
        const opt<std::string> a{"a"};
        const opt<std::string> b{"b"};
        CHECK(a == a);
        CHECK(a != b);
        CHECK(a == "a");
        CHECK(a != "b");
        CHECK_FALSE(a != a);
        CHECK_FALSE(a == b);
        CHECK_FALSE(a != "a");
        CHECK_FALSE(a == "b");
    }

    SECTION("null <-> null")
    {
        const opt<std::string> a;
        const opt<std::string> b;
        CHECK(a == a);
        CHECK(a == b);
        CHECK_FALSE(a != a);
        CHECK_FALSE(a != b);
    }

    SECTION("value <-> null")
    {
        const opt<std::string> a{"a"};
        const opt<std::string> b;
        CHECK(a != b);
        CHECK_FALSE(a == b);
    }

    SECTION("null <-> value")
    {
        const opt<std::string> a;
        const opt<std::string> b{"b"};
        CHECK(a != b);
        CHECK(a != "1");
        CHECK(a != std::string("1"));
        CHECK_FALSE(a == b);
        CHECK_FALSE(a == "1");
        CHECK_FALSE(a == std::string("1"));
    }
}
