/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include <array>
#include <algorithm>
#include <utility>
#include <catch2/catch.hpp>
#include <cppwamp/internal/surrogateany.hpp>

using namespace wamp;
using namespace wamp::internal;

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
struct SurrogateAnyTestAccess
{
    SurrogateAnyTestAccess(const SurrogateAny& a) : any_(a) {}

    bool isLocal() const {return any_.isLocal();}

private:
    const SurrogateAny& any_;
};

} // namespace internal

} // namespace wamp


namespace
{

//------------------------------------------------------------------------------
void checkSurrogateAnyIsEmpty(const SurrogateAny& a)
{
    CHECK_FALSE( a.has_value() );
    CHECK( a.type() == typeid(void) );
    CHECK( anyCast<int>(&a) == nullptr );
    CHECK_THROWS_AS( anyCast<int>(a), BadAnyCast );
}

//------------------------------------------------------------------------------
void checkSurrogateAnyIsEmpty(const SurrogateAny& a, const std::string& info)
{
    INFO(info);
    checkSurrogateAnyIsEmpty(a);
}

//------------------------------------------------------------------------------
template <typename T>
void checkSurrogateAnyValue(const SurrogateAny& a, const T& x, bool isLocal)
{
    SurrogateAnyTestAccess t(a);
    CHECK( a.has_value() );
    CHECK( a.type() == typeid(T) );
    CHECK( t.isLocal() == isLocal );
    auto ptr = anyCast<T>(&a);
    REQUIRE( ptr != nullptr );
    CHECK( *ptr == x );
    CHECK( anyCast<T>(a) == x );
}

//------------------------------------------------------------------------------
template <typename T>
void checkSurrogateAnyValue(const SurrogateAny& a, const T& x, bool isLocal,
                            const std::string& info)
{
    INFO(info);
    checkSurrogateAnyValue(a, x, isLocal);
}

//------------------------------------------------------------------------------
struct Small
{
    Small(int n = 0) : value(n), valueConstructed(true) {}

    Small(const Small& rhs) : value(rhs.value), copyConstructed(true) {}

    Small(Small&& rhs) noexcept
        : value(rhs.value), moveConstructed(true)
    {
        rhs.movedFrom = true;
    }

    Small& operator=(const Small& rhs)
    {
        value = rhs.value;
        copyAssigned = true;
        return *this;
    }

    Small& operator=(Small&& rhs) noexcept
    {
        value = rhs.value;
        moveAssigned = true;
        rhs.movedFrom = true;
        return *this;
    }

    bool operator==(const Small& rhs) const {return value == rhs.value;}

    int value = 0;
    bool valueConstructed = false;
    bool copyConstructed = false;
    bool moveConstructed = false;
    bool copyAssigned = false;
    bool moveAssigned = false;
    bool movedFrom = false;
};

//------------------------------------------------------------------------------
struct Large
{
    Large(char n = 0) : valueConstructed(true)
    {
        std::iota(array.begin(), array.end(), n);
    }

    Large(const Large& rhs) : array(rhs.array), copyConstructed(true) {}

    Large(Large&& rhs) noexcept
        : array(rhs.array), moveConstructed(true)
    {
        rhs.movedFrom = true;
    }

    Large& operator=(const Large& rhs)
    {
        array = rhs.array;
        copyAssigned = true;
        return *this;
    }

    Large& operator=(Large&& rhs) noexcept
    {
        array = rhs.array;
        moveAssigned = true;
        rhs.movedFrom = true;
        return *this;
    }

    bool operator==(const Large& rhs) const {return array == rhs.array;}

    std::array<char, 2*sizeof(SurrogateAny)> array;
    bool valueConstructed = false;
    bool copyConstructed = false;
    bool moveConstructed = false;
    bool copyAssigned = false;
    bool moveAssigned = false;
    bool movedFrom = false;
};

} // anonymous namespace


//------------------------------------------------------------------------------
TEST_CASE( "SurrogateAny Value Construction", "[SurrogateAny]" )
{
    SECTION( "default constructor" )
    {
        SurrogateAny a;
        checkSurrogateAnyIsEmpty(a);
    }

    SECTION( "small object" )
    {
        Small x(42);
        SurrogateAny a(x);
        checkSurrogateAnyValue(a, x, true);
    }

    SECTION( "pointer pair" )
    {
        using Pair = std::pair<int*, float*>;
        int n = 42;
        float x = 12.34;
        Pair p{&n, &x};
        SurrogateAny a(p);
        checkSurrogateAnyValue(a, p, true);
        auto ptr = anyCast<Pair>(a);
        CHECK( *(ptr.first) == n );
        CHECK( *(ptr.second) == x );
    }

    SECTION( "large object" )
    {
        Large x;
        SurrogateAny a(x);
        checkSurrogateAnyValue(a, x, false);
    }

    SECTION( "object that is non-alignable in local storage" )
    {
        using Type = long double;
        Type x = 123.45;
        SurrogateAny a(x);
        checkSurrogateAnyValue(a, x, false);
    }
}

//------------------------------------------------------------------------------
TEST_CASE( "SurrogateAny Emplacement", "[SurrogateAny]" )
{
    SECTION( "small object" )
    {
        using Pair = std::pair<int, float>;
        Pair p = {12, 34.56};

        SurrogateAny a(InPlaceType<Pair>{}, p.first, p.second);
        checkSurrogateAnyValue(a, p, true, "a");

        SurrogateAny b;
        b.emplace<Pair>(p.first, p.second);
        checkSurrogateAnyValue(b, p, true, "b");
    }

    SECTION( "small object with initializer list" )
    {
        struct Foo
        {
            int sum;
            float x;

            Foo(std::initializer_list<int> list, float x)
                : sum(0),
                  x(x)
            {
                for (auto n: list)
                    sum += n;
            }

            bool operator==(const Foo& rhs) const
            {
                return sum == rhs.sum && x == rhs.x;
            }
        };

        Foo foo({12, 34}, 56.78);

        SurrogateAny a(InPlaceType<Foo>{}, {12, 34}, foo.x);
        checkSurrogateAnyValue(a, foo, true, "a");

        SurrogateAny b;
        b.emplace<Foo>({12, 34}, foo.x);
        checkSurrogateAnyValue(b, foo, true, "b");
    }

    SECTION( "large object" )
    {
        using Array = std::array<char, 2*sizeof(SurrogateAny)>;
        using Type = std::pair<Array, float>;
        Type p;
        std::iota(p.first.begin(), p.first.end(), 0);
        p.second = 12.34;

        SurrogateAny a(InPlaceType<Type>{}, p.first, p.second);
        checkSurrogateAnyValue(a, p, false, "a");

        SurrogateAny b;
        b.emplace<Type>(p.first, p.second);
        checkSurrogateAnyValue(b, p, false, "b");
    }

    SECTION( "object that is non-alignable in local storage" )
    {
        using Type = long double;
        Type x = 123.45;

        SurrogateAny a(InPlaceType<Type>{}, x);
        checkSurrogateAnyValue(a, x, false, "a");

        SurrogateAny b;
        b.emplace<Type>(x);
        checkSurrogateAnyValue(b, x, false, "b");
    }

    SECTION( "large object with initializer list" )
    {
        using Array = std::array<char, 2*sizeof(SurrogateAny)>;

        struct Foo
        {
            int sum;
            Array array;

            Foo(std::initializer_list<int> list, Array a)
                : sum(0),
                  array(a)
            {
                for (auto n: list)
                    sum += n;
            }

            bool operator==(const Foo& rhs) const
            {
                return sum == rhs.sum && array == rhs.array;
            }
        };

        Array s;
        std::iota(s.begin(), s.end(), 0);
        Foo foo({12, 34}, s);

        SurrogateAny a(InPlaceType<Foo>{}, {12, 34}, s);
        checkSurrogateAnyValue(a, foo, false, "a");

        SurrogateAny b;
        b.emplace<Foo>({12, 34}, s);
        checkSurrogateAnyValue(b, foo, false, "b");
    }
}

//------------------------------------------------------------------------------
TEST_CASE( "SurrogateAny Copy and Move Construction", "[SurrogateAny]" )
{
    SECTION( "copy empty rhs" )
    {
        SurrogateAny rhs;
        SurrogateAny lhs(rhs);
        checkSurrogateAnyIsEmpty(lhs, "lhs");
        checkSurrogateAnyIsEmpty(rhs, "rhs");
    }

    SECTION( "copy small rhs" )
    {
        Small x(42);
        SurrogateAny rhs(InPlaceType<Small>{}, x.value);
        SurrogateAny lhs(rhs);
        checkSurrogateAnyValue(lhs, x, true, "lhs");
        checkSurrogateAnyValue(rhs, x, true, "rh");
        CHECK( anyCast<const Small&>(lhs).copyConstructed );
        CHECK_FALSE( anyCast<const Small&>(rhs).movedFrom );
    }

    SECTION( "copy large rhs" )
    {
        Large x;
        SurrogateAny rhs(InPlaceType<Large>{}, x.array.front());
        SurrogateAny lhs(rhs);
        checkSurrogateAnyValue(lhs, x, false, "lhs");
        checkSurrogateAnyValue(rhs, x, false, "rhs");
        CHECK( anyCast<const Large&>(lhs).copyConstructed );
        CHECK_FALSE( anyCast<const Large&>(rhs).movedFrom );
    }

    SECTION( "move empty rhs" )
    {
        SurrogateAny rhs;
        SurrogateAny lhs(std::move(rhs));
        checkSurrogateAnyIsEmpty(lhs, "lhs");
        checkSurrogateAnyIsEmpty(rhs, "rhs");
    }

    SECTION( "move small rhs" )
    {
        Small x(42);
        SurrogateAny rhs(InPlaceType<Small>{}, x.value);
        SurrogateAny lhs(std::move(rhs));
        checkSurrogateAnyValue(lhs, x, true);
        checkSurrogateAnyIsEmpty(rhs);
        CHECK( anyCast<const Small&>(lhs).moveConstructed );
    }

    SECTION( "move large rhs" )
    {
        Large x;
        SurrogateAny rhs(InPlaceType<Large>{}, x.array.front());
        SurrogateAny lhs(std::move(rhs));
        checkSurrogateAnyValue(lhs, x, false);
        checkSurrogateAnyIsEmpty(rhs);

        // rhs.box_ pointer is directly assigned to lhs.box_, so underlying
        // value's copy constructor is never invoked.
        CHECK( anyCast<const Large&>(lhs).valueConstructed );
    }
}

//------------------------------------------------------------------------------
TEST_CASE( "SurrogateAny Assignment and Reset", "[SurrogateAny]" )
{
    Small small(42);
    Large large;

    SECTION( "empty lhs" )
    {
        SurrogateAny lhs;

        SECTION( "reset" )
        {
            lhs.reset();
            checkSurrogateAnyIsEmpty(lhs);
        }

        SECTION( "copy small rhs" )
        {
            lhs = small;
            checkSurrogateAnyValue(lhs, small, true);
            CHECK_FALSE( small.movedFrom );

            // Underlying value move-constructed from temporary AnySurrogate
            CHECK( anyCast<const Small&>(lhs).moveConstructed );
        }

        SECTION( "copy large rhs" )
        {
            lhs = large;
            checkSurrogateAnyValue(lhs, large, false);
            CHECK_FALSE( large.movedFrom );

            // box_ pointer directly assigned from temporary AnySurrogate
            // that was copy-constructed
            CHECK( anyCast<const Large&>(lhs).copyConstructed);
        }

        SECTION( "move small rhs" )
        {
            lhs = std::move(small);
            checkSurrogateAnyValue(lhs, small, true);
            CHECK( small.movedFrom );

            // Underlying value move-constructed from temporary AnySurrogate
            CHECK( anyCast<const Small&>(lhs).moveConstructed );
        }

        SECTION( "move large rhs" )
        {
            lhs = std::move(large);
            checkSurrogateAnyValue(lhs, large, false);
            CHECK( large.movedFrom );

            // box_ pointer directly assigned from temporary AnySurrogate
            // that was move-constructed
            CHECK( anyCast<const Large&>(lhs).moveConstructed );
        }
    }

    SECTION( "small lhs" )
    {
        SurrogateAny lhs(small);

        SECTION( "reset" )
        {
            lhs.reset();
            checkSurrogateAnyIsEmpty(lhs);
        }

        SECTION( "copy small rhs" )
        {
            lhs = small;
            checkSurrogateAnyValue(lhs, small, true);
            CHECK_FALSE( small.movedFrom );

            // Underlying value move-constructed from temporary AnySurrogate
            CHECK( anyCast<const Small&>(lhs).moveConstructed );
        }

        SECTION( "copy large rhs" )
        {
            lhs = large;
            checkSurrogateAnyValue(lhs, large, false);
            CHECK_FALSE( large.movedFrom );

            // box_ pointer directly assigned from temporary AnySurrogate
            // that was copy-constructed
            CHECK( anyCast<const Large&>(lhs).copyConstructed);
        }

        SECTION( "move small rhs" )
        {
            lhs = std::move(small);
            checkSurrogateAnyValue(lhs, small, true);
            CHECK( small.movedFrom );

            // Underlying value move-constructed from temporary AnySurrogate
            CHECK( anyCast<const Small&>(lhs).moveConstructed );
        }

        SECTION( "move large rhs" )
        {
            lhs = std::move(large);
            checkSurrogateAnyValue(lhs, large, false);
            CHECK( large.movedFrom );

            // box_ pointer directly assigned from temporary AnySurrogate
            // that was move-constructed
            CHECK( anyCast<const Large&>(lhs).moveConstructed );
        }
    }

    SECTION( "large lhs" )
    {
        SurrogateAny lhs(large);

        SECTION( "reset" )
        {
            lhs.reset();
            checkSurrogateAnyIsEmpty(lhs);
        }

        SECTION( "copy small rhs" )
        {
            lhs = small;
            checkSurrogateAnyValue(lhs, small, true);
            CHECK_FALSE( small.movedFrom );

            // Underlying value move-constructed from temporary AnySurrogate
            CHECK( anyCast<const Small&>(lhs).moveConstructed );
        }

        SECTION( "copy large rhs" )
        {
            lhs = large;
            checkSurrogateAnyValue(lhs, large, false);
            CHECK_FALSE( large.movedFrom );

            // box_ pointer directly assigned from temporary AnySurrogate
            // that was copy-constructed
            CHECK( anyCast<const Large&>(lhs).copyConstructed);
        }

        SECTION( "move small rhs" )
        {
            lhs = std::move(small);
            checkSurrogateAnyValue(lhs, small, true);
            CHECK( small.movedFrom );

            // Underlying value move-constructed from temporary AnySurrogate
            CHECK( anyCast<const Small&>(lhs).moveConstructed );
        }

        SECTION( "move large rhs" )
        {
            lhs = std::move(large);
            checkSurrogateAnyValue(lhs, large, false);
            CHECK( large.movedFrom );

            // box_ pointer directly assigned from temporary AnySurrogate
            // that was move-constructed
            CHECK( anyCast<const Large&>(lhs).moveConstructed );
        }
    }
}

//------------------------------------------------------------------------------
TEST_CASE( "SurrogateAny Copy and Move Assignment", "[SurrogateAny]" )
{
    Small small(42);
    Large large;

    SECTION( "empty lhs" )
    {
        SurrogateAny lhs;

        SECTION( "copy empty rhs" )
        {
            SurrogateAny rhs;
            lhs = rhs;
            checkSurrogateAnyIsEmpty(lhs, "lhs");
            checkSurrogateAnyIsEmpty(rhs, "rhs");
        }

        SECTION( "copy small rhs" )
        {
            SurrogateAny rhs(small);
            lhs = rhs;
            checkSurrogateAnyValue(lhs, small, true);
            checkSurrogateAnyValue(rhs, small, true);
            CHECK_FALSE( anyCast<const Small&>(rhs).movedFrom );

            // Underlying value move-constructed from temporary AnySurrogate
            CHECK( anyCast<const Small&>(lhs).moveConstructed);
        }

        SECTION( "copy large rhs" )
        {
            SurrogateAny rhs(large);
            lhs = rhs;
            checkSurrogateAnyValue(lhs, large, false);
            checkSurrogateAnyValue(rhs, large, false);
            CHECK_FALSE( anyCast<const Large&>(rhs).movedFrom );

            // box_ pointer directly assigned from temporary AnySurrogate
            // that was copy-constructed
            CHECK( anyCast<const Large&>(lhs).copyConstructed );
        }

        SECTION( "move empty rhs" )
        {
            SurrogateAny rhs;
            lhs = std::move(rhs);
            checkSurrogateAnyIsEmpty(lhs, "lhs");
            checkSurrogateAnyIsEmpty(rhs, "rhs");
        }

        SECTION( "move small rhs" )
        {
            SurrogateAny rhs(small);
            lhs = std::move(rhs);
            checkSurrogateAnyValue(lhs, small, true);
            checkSurrogateAnyIsEmpty(rhs, "rhs");

            // Underlying value move-constructed from rhs
            CHECK( anyCast<const Small&>(lhs).moveConstructed );
        }

        SECTION( "move large rhs" )
        {
            SurrogateAny rhs(large);
            lhs = std::move(rhs);
            checkSurrogateAnyValue(lhs, large, false);
            checkSurrogateAnyIsEmpty(rhs, "rhs");

            // box_ pointer directly assigned from rhs that copy-constructed
            // its underdying value
            CHECK( anyCast<const Large&>(lhs).copyConstructed );
        }
    }

    SECTION( "small lhs" )
    {
        SurrogateAny lhs(small);

        SECTION( "copy empty rhs" )
        {
            SurrogateAny rhs;
            lhs = rhs;
            checkSurrogateAnyIsEmpty(lhs, "lhs");
            checkSurrogateAnyIsEmpty(rhs, "rhs");
        }

        SECTION( "copy small rhs" )
        {
            SurrogateAny rhs(small);
            lhs = rhs;
            checkSurrogateAnyValue(lhs, small, true, "lhs");
            checkSurrogateAnyValue(rhs, small, true, "rhs");
            CHECK_FALSE( anyCast<const Small&>(rhs).movedFrom );

            // Underlying value move-constructed from temporary AnySurrogate
            CHECK( anyCast<const Small&>(lhs).moveConstructed );
        }

        SECTION( "copy large rhs" )
        {
            SurrogateAny rhs(large);
            lhs = rhs;
            checkSurrogateAnyValue(lhs, large, false, "lhs");
            checkSurrogateAnyValue(rhs, large, false, "rhs");
            CHECK_FALSE( anyCast<const Large&>(rhs).movedFrom );

            // box_ pointer directly assigned from temporary AnySurrogate
            // that was copy-constructed
            CHECK( anyCast<const Large&>(lhs).copyConstructed);
        }

        SECTION( "move empty rhs" )
        {
            SurrogateAny rhs;
            lhs = std::move(rhs);
            checkSurrogateAnyIsEmpty(lhs, "lhs");
            checkSurrogateAnyIsEmpty(rhs, "rhs");
        }

        SECTION( "move small rhs" )
        {
            SurrogateAny rhs(small);
            lhs = std::move(rhs);
            checkSurrogateAnyValue(lhs, small, true);
            checkSurrogateAnyIsEmpty(rhs);

            // Underlying value move-constructed from rhs
            CHECK( anyCast<const Small&>(lhs).moveConstructed );
        }

        SECTION( "move large rhs" )
        {
            SurrogateAny rhs(large);
            lhs = std::move(rhs);
            checkSurrogateAnyValue(lhs, large, false);
            checkSurrogateAnyIsEmpty(rhs);

            // box_ pointer directly assigned from rhs that copy-constructed
            // its underdying value
            CHECK( anyCast<const Large&>(lhs).copyConstructed );
        }
    }

    SECTION( "large lhs" )
    {
        SurrogateAny lhs(large);

        SECTION( "copy empty rhs" )
        {
            SurrogateAny rhs;
            lhs = rhs;
            checkSurrogateAnyIsEmpty(lhs, "lhs");
            checkSurrogateAnyIsEmpty(rhs, "rhs");
        }

        SECTION( "copy small rhs" )
        {
            SurrogateAny rhs(small);
            lhs = rhs;
            checkSurrogateAnyValue(lhs, small, true, "lhs");
            checkSurrogateAnyValue(rhs, small, true, "rhs");
            CHECK_FALSE( anyCast<const Small&>(rhs).movedFrom );

            // Underlying value move-constructed from temporary AnySurrogate
            CHECK( anyCast<const Small&>(lhs).moveConstructed );
        }

        SECTION( "copy large rhs" )
        {
            SurrogateAny rhs(large);
            lhs = rhs;
            checkSurrogateAnyValue(lhs, large, false, "lhs");
            checkSurrogateAnyValue(rhs, large, false, "rhs");
            CHECK_FALSE( anyCast<const Large&>(rhs).movedFrom );

            // box_ pointer directly assigned from temporary AnySurrogate
            // that was copy-constructed
            CHECK( anyCast<const Large&>(lhs).copyConstructed);
        }

        SECTION( "move empty rhs" )
        {
            SurrogateAny rhs;
            lhs = std::move(rhs);
            checkSurrogateAnyIsEmpty(lhs, "lhs");
            checkSurrogateAnyIsEmpty(rhs, "rhs");
        }

        SECTION( "move small rhs" )
        {
            SurrogateAny rhs(small);
            lhs = std::move(rhs);
            checkSurrogateAnyValue(lhs, small, true);
            checkSurrogateAnyIsEmpty(rhs);

            // Underlying value move-constructed from rhs
            CHECK( anyCast<const Small&>(lhs).moveConstructed );
        }

        SECTION( "move large rhs" )
        {
            SurrogateAny rhs(large);
            lhs = std::move(rhs);
            checkSurrogateAnyValue(lhs, large, false);
            checkSurrogateAnyIsEmpty(rhs);

            // box_ pointer directly assigned from rhs that copy-constructed
            // its underdying value
            CHECK( anyCast<const Large&>(lhs).copyConstructed );
        }
    }
}

//------------------------------------------------------------------------------
TEST_CASE( "SurrogateAny Swap", "[SurrogateAny]" )
{
    int small = 42;
    int small2 = 24;
    std::array<char, 2*sizeof(SurrogateAny)> large;
    std::iota(large.begin(), large.end(), 0);
    std::array<char, 2*sizeof(SurrogateAny)> large2;
    std::iota(large.begin(), large.end(), 1);

    SECTION( "empty lhs" )
    {
        SurrogateAny lhs;

        SECTION( "empty rhs" )
        {
            SurrogateAny rhs;
            swap(lhs, rhs);
            checkSurrogateAnyIsEmpty(lhs, "lhs");
            checkSurrogateAnyIsEmpty(rhs, "rhs");
        }

        SECTION( "small rhs" )
        {
            SurrogateAny rhs(small);
            swap(lhs, rhs);
            checkSurrogateAnyValue(lhs, small, true);
            checkSurrogateAnyIsEmpty(rhs);
        }

        SECTION( "large rhs" )
        {
            SurrogateAny rhs(large);
            swap(lhs, rhs);
            checkSurrogateAnyValue(lhs, large, false);
            checkSurrogateAnyIsEmpty(rhs);
        }
    }

    SECTION( "small lhs" )
    {
        SurrogateAny lhs(small);

        SECTION( "empty rhs" )
        {
            SurrogateAny rhs;
            swap(lhs, rhs);
            checkSurrogateAnyIsEmpty(lhs);
            checkSurrogateAnyValue(rhs, small, true);
        }

        SECTION( "small rhs" )
        {
            SurrogateAny rhs(small2);
            swap(lhs, rhs);
            checkSurrogateAnyValue(lhs, small2, true);
            checkSurrogateAnyValue(rhs, small, true);
        }

        SECTION( "large rhs" )
        {
            SurrogateAny rhs(large);
            swap(lhs, rhs);
            checkSurrogateAnyValue(lhs, large, false);
            checkSurrogateAnyValue(rhs, small, true);
        }
    }

    SECTION( "large lhs" )
    {
        SurrogateAny lhs(large);

        SECTION( "empty rhs" )
        {
            SurrogateAny rhs;
            swap(lhs, rhs);
            checkSurrogateAnyIsEmpty(lhs);
            checkSurrogateAnyValue(rhs, large, false);
        }

        SECTION( "small rhs" )
        {
            SurrogateAny rhs(small);
            swap(lhs, rhs);
            checkSurrogateAnyValue(lhs, small, true);
            checkSurrogateAnyValue(rhs, large, false);
        }

        SECTION( "large rhs" )
        {
            SurrogateAny rhs(large2);
            swap(lhs, rhs);
            checkSurrogateAnyValue(lhs, large2, false);
            checkSurrogateAnyValue(rhs, large, false);
        }
    }
}

//------------------------------------------------------------------------------
TEST_CASE( "Valid SurrogateAny Casts", "[SurrogateAny]" )
{
    // TODO
}

//------------------------------------------------------------------------------
TEST_CASE( "Bad SurrogateAny Casts", "[SurrogateAny]" )
{
    // TODO
}
