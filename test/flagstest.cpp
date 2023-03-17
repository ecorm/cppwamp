/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include <utility>
#include <catch2/catch.hpp>
#include <cppwamp/flags.hpp>

using namespace wamp;

enum class TestEnum : unsigned
{
    zero      = 0,
    one       = 1 << 0,
    two       = 1 << 1,
    oneAndTwo = one | two
};

namespace wamp { template <> struct IsFlag<TestEnum> : TrueType {}; }

//------------------------------------------------------------------------------
SCENARIO( "Constructing Flags", "[Flags]" )
{
GIVEN( "a default-constructed Flags instance" )
{
    Flags<TestEnum> f;
    THEN( "no flags are set" )
    {
        CHECK_FALSE( (bool)f );
        CHECK      ( f == TestEnum::zero );
        CHECK_FALSE( f == TestEnum::one );
        CHECK_FALSE( f == TestEnum::two );
        CHECK_FALSE( f == TestEnum::oneAndTwo );
        CHECK      ( f.test(TestEnum::zero) );
        CHECK_FALSE( f.test(TestEnum::one) );
        CHECK_FALSE( f.test(TestEnum::two) );
        CHECK      ( f.none() );
    }
}
GIVEN( "an enumerator" )
{
    TestEnum e = TestEnum::two;
    WHEN( "constructed from an enumerator" )
    {
        Flags<TestEnum> f(e);
        THEN( "only the flag for that enumerator is set" )
        {
            CHECK      ( (bool)f );
            CHECK_FALSE( f == TestEnum::zero );
            CHECK_FALSE( f == TestEnum::one );
            CHECK      ( f == TestEnum::two );
            CHECK_FALSE( f == TestEnum::oneAndTwo );
            CHECK      ( f.test(TestEnum::zero) );
            CHECK_FALSE( f.test(TestEnum::one) );
            CHECK      ( f.test(TestEnum::two) );
            CHECK_FALSE( f.none() );
        }
    }
}
GIVEN( "a null enumerator" )
{
    TestEnum e = TestEnum::zero;
    WHEN( "constructed from a null enumerator" )
    {
        Flags<TestEnum> f(e);
        THEN( "no flags are set" )
        {
            CHECK_FALSE( (bool)f );
            CHECK      ( f == TestEnum::zero );
            CHECK_FALSE( f == TestEnum::one );
            CHECK_FALSE( f == TestEnum::two );
            CHECK_FALSE( f == TestEnum::oneAndTwo );
            CHECK      ( f.test(TestEnum::zero) );
            CHECK_FALSE( f.test(TestEnum::one) );
            CHECK_FALSE( f.test(TestEnum::two) );
            CHECK      ( f.none() );
        }
    }
}
GIVEN( "another Flags instance" )
{
    TestEnum e = TestEnum::one;
    Flags<TestEnum> rhs(e);
    WHEN( "copy constructed" )
    {
        Flags<TestEnum> lhs(rhs);
        THEN( "only the appropriate flags are set" )
        {
            CHECK      ( (bool)lhs );
            CHECK_FALSE( lhs == TestEnum::zero );
            CHECK      ( lhs == TestEnum::one );
            CHECK_FALSE( lhs == TestEnum::two );
            CHECK_FALSE( lhs == TestEnum::oneAndTwo );
            CHECK      ( lhs.test(TestEnum::zero) );
            CHECK      ( lhs.test(TestEnum::one) );
            CHECK_FALSE( lhs.test(TestEnum::two) );
            CHECK_FALSE( lhs.none() );
        }
        AND_WHEN( "the copy constructed instance is modified" )
        {
            lhs.set(TestEnum::two);
            THEN( "the original instance remains enchanged" )
            {
                CHECK      ( (bool)rhs );
                CHECK_FALSE( rhs == TestEnum::zero );
                CHECK      ( rhs == TestEnum::one );
                CHECK_FALSE( rhs == TestEnum::two );
                CHECK_FALSE( rhs == TestEnum::oneAndTwo );
                CHECK      ( rhs.test(TestEnum::zero) );
                CHECK      ( rhs.test(TestEnum::one) );
                CHECK_FALSE( rhs.test(TestEnum::two) );
                CHECK_FALSE( rhs.none() );
            }
        }
    }
    WHEN( "move constructed" )
    {
        Flags<TestEnum> lhs(std::move(rhs));
        THEN( "only the appropriate flags are set" )
        {
            CHECK      ( (bool)lhs );
            CHECK_FALSE( lhs == TestEnum::zero );
            CHECK      ( lhs == TestEnum::one );
            CHECK_FALSE( lhs == TestEnum::two );
            CHECK_FALSE( lhs == TestEnum::oneAndTwo );
            CHECK      ( lhs.test(TestEnum::zero) );
            CHECK      ( lhs.test(TestEnum::one) );
            CHECK_FALSE( lhs.test(TestEnum::two) );
            CHECK_FALSE( lhs.none() );
        }
    }
}
GIVEN( "an integer" )
{
    auto e = TestEnum::oneAndTwo;
    auto n = static_cast<std::underlying_type<TestEnum>::type>(e);
    WHEN( "constructing with a raw integer" )
    {
        Flags<TestEnum> f(in_place, n);
        THEN( "only the appropriate flags are set" )
        {
            CHECK      ( (bool)f );
            CHECK_FALSE( f == TestEnum::zero );
            CHECK_FALSE( f == TestEnum::one );
            CHECK_FALSE( f == TestEnum::two );
            CHECK      ( f == TestEnum::oneAndTwo );
            CHECK      ( f.test(TestEnum::zero) );
            CHECK      ( f.test(TestEnum::one) );
            CHECK      ( f.test(TestEnum::two) );
            CHECK_FALSE( f.none() );
        }
    }
}
}

//------------------------------------------------------------------------------
SCENARIO( "Assigning Flags", "[Flags]" )
{
GIVEN( "two Flags operands" )
{
    Flags<TestEnum> lhs(TestEnum::one);
    Flags<TestEnum> rhs(TestEnum::two);
    WHEN( "the RHS is copy-assigned to the LHS" )
    {
        lhs = rhs;
        THEN( "the LHS has the same bitfield as the RHS" )
        {
            CHECK( lhs == TestEnum::two );
            CHECK( lhs == rhs );
            CHECK      ( lhs == TestEnum::two );
            CHECK_FALSE( lhs.test(TestEnum::one) );
            CHECK      ( lhs.test(TestEnum::two) );
            CHECK_FALSE( lhs.none() );
        }
        AND_WHEN( "the LHS is modified" )
        {
            lhs.set(TestEnum::one);
            lhs.reset(TestEnum::two);
            THEN( "the RHS remains unchanged" )
            {
                CHECK      ( rhs == TestEnum::two );
                CHECK_FALSE( rhs.test(TestEnum::one) );
                CHECK      ( rhs.test(TestEnum::two) );
                CHECK_FALSE( rhs.none() );
            }
        }
    }
    WHEN( "the RHS is move-assigned to the LHS" )
    {
        lhs = std::move(rhs);
        THEN( "the LHS has the bitfield that RHS used to have" )
        {
            CHECK      ( lhs == TestEnum::two );
            CHECK_FALSE( lhs.test(TestEnum::one) );
            CHECK      ( lhs.test(TestEnum::two) );
            CHECK_FALSE( lhs.none() );
        }
    }
}
}

//------------------------------------------------------------------------------
SCENARIO( "Setting flags, testing flags, and comparison", "[Flags]" )
{
GIVEN( "an empty Flags instance" )
{
    Flags<TestEnum> f;
    WHEN( "a flag is set" )
    {
        f.set(TestEnum::one);
        THEN( "that flag tests true" )
        {
            CHECK_FALSE( f == TestEnum::zero );
            CHECK      ( f != TestEnum::zero );
            CHECK      ( f == TestEnum::one );
            CHECK_FALSE( f != TestEnum::one );
            CHECK_FALSE( f == TestEnum::two );
            CHECK      ( f != TestEnum::two );
            CHECK_FALSE( f == TestEnum::oneAndTwo );
            CHECK      ( f != TestEnum::oneAndTwo );
            CHECK      ( (bool)f );
            CHECK_FALSE( f.none() );
            CHECK      ( f.test(TestEnum::one ));
            CHECK_FALSE( f.test(TestEnum::two ));
        }
        AND_WHEN( "another flag is set" )
        {
            f.set(TestEnum::two);
            THEN( "both flags test true" )
            {
                CHECK_FALSE( f == TestEnum::zero );
                CHECK      ( f != TestEnum::zero );
                CHECK_FALSE( f == TestEnum::one );
                CHECK      ( f != TestEnum::one );
                CHECK_FALSE( f == TestEnum::two );
                CHECK      ( f != TestEnum::two );
                CHECK      ( f == TestEnum::oneAndTwo );
                CHECK_FALSE( f != TestEnum::oneAndTwo );
                CHECK      ( (bool)f );
                CHECK_FALSE( f.none() );
                CHECK      ( f.test(TestEnum::one ));
                CHECK      ( f.test(TestEnum::two ));
            }
        }
        AND_WHEN( "a null flag is set" )
        {
            f.set(TestEnum::zero);
            THEN( "nothing changes" )
            {
                CHECK_FALSE( f == TestEnum::zero );
                CHECK      ( f != TestEnum::zero );
                CHECK      ( f == TestEnum::one );
                CHECK_FALSE( f != TestEnum::one );
                CHECK_FALSE( f == TestEnum::two );
                CHECK      ( f != TestEnum::two );
                CHECK_FALSE( f == TestEnum::oneAndTwo );
                CHECK      ( f != TestEnum::oneAndTwo );
                CHECK      ( (bool)f );
                CHECK_FALSE( f.none() );
                CHECK      ( f.test(TestEnum::one ));
                CHECK_FALSE( f.test(TestEnum::two ));
            }
        }
        AND_WHEN( "the same flag is set again" )
        {
            f.set(TestEnum::one);
            THEN( "nothing changes" )
            {
                CHECK_FALSE( f == TestEnum::zero );
                CHECK      ( f != TestEnum::zero );
                CHECK      ( f == TestEnum::one );
                CHECK_FALSE( f != TestEnum::one );
                CHECK_FALSE( f == TestEnum::two );
                CHECK      ( f != TestEnum::two );
                CHECK_FALSE( f == TestEnum::oneAndTwo );
                CHECK      ( f != TestEnum::oneAndTwo );
                CHECK      ( (bool)f );
                CHECK_FALSE( f.none() );
                CHECK      ( f.test(TestEnum::one ));
                CHECK_FALSE( f.test(TestEnum::two ));
            }
        }
    }
    WHEN( "a different flag is set" )
    {
        f.set(TestEnum::two);
        THEN( "that flag tests true" )
        {
            CHECK_FALSE( f == TestEnum::zero );
            CHECK      ( f != TestEnum::zero );
            CHECK_FALSE( f == TestEnum::one );
            CHECK      ( f != TestEnum::one );
            CHECK      ( f == TestEnum::two );
            CHECK_FALSE( f != TestEnum::two );
            CHECK_FALSE( f == TestEnum::oneAndTwo );
            CHECK      ( f != TestEnum::oneAndTwo );
            CHECK      ( (bool)f );
            CHECK_FALSE( f.none() );
            CHECK_FALSE( f.test(TestEnum::one ));
            CHECK      ( f.test(TestEnum::two ));
        }
    }
    WHEN( "a null flag is set" )
    {
        f.set(TestEnum::zero);
        THEN( "nothing changes" )
        {
            CHECK      ( f == TestEnum::zero );
            CHECK_FALSE( f != TestEnum::zero );
            CHECK_FALSE( f == TestEnum::one );
            CHECK      ( f != TestEnum::one );
            CHECK_FALSE( f == TestEnum::two );
            CHECK      ( f != TestEnum::two );
            CHECK_FALSE( f == TestEnum::oneAndTwo );
            CHECK      ( f != TestEnum::oneAndTwo );
            CHECK_FALSE( (bool)f );
            CHECK      ( f.none() );
            CHECK_FALSE( f.test(TestEnum::one ));
            CHECK_FALSE( f.test(TestEnum::two ));
        }
    }
    WHEN( "two flags are simulateously set" )
    {
        f.set(TestEnum::oneAndTwo);
        THEN( "both flags are set" )
        {
            CHECK_FALSE( f == TestEnum::zero );
            CHECK      ( f != TestEnum::zero );
            CHECK_FALSE( f == TestEnum::one );
            CHECK      ( f != TestEnum::one );
            CHECK_FALSE( f == TestEnum::two );
            CHECK      ( f != TestEnum::two );
            CHECK      ( f == TestEnum::oneAndTwo );
            CHECK_FALSE( f != TestEnum::oneAndTwo );
            CHECK      ( (bool)f );
            CHECK_FALSE( f.none() );
            CHECK      ( f.test(TestEnum::one ));
            CHECK      ( f.test(TestEnum::two ));
        }
    }
}
}

//------------------------------------------------------------------------------
SCENARIO( "Clearing all flags", "[Flags]" )
{
GIVEN( "a non-empty Flags instance" )
{
    Flags<TestEnum> f(TestEnum::one);
    WHEN( "the Flags are all cleared" )
    {
        f.reset();
        THEN( "none of the flags are set" )
        {
            CHECK      ( f == TestEnum::zero );
            CHECK_FALSE( f == TestEnum::one );
            CHECK_FALSE( f == TestEnum::two );
            CHECK_FALSE( f == TestEnum::oneAndTwo );
            CHECK_FALSE( (bool)f );
            CHECK      ( f.none() );
            CHECK_FALSE( f.test(TestEnum::one ));
            CHECK_FALSE( f.test(TestEnum::two ));
        }
    }
}
GIVEN( "an empty Flags instance" )
{
    Flags<TestEnum> f;
    WHEN( "the Flags are cleared" )
    {
        f.reset();
        THEN( "none of the flags are set" )
        {
            CHECK      ( f == TestEnum::zero );
            CHECK_FALSE( f == TestEnum::one );
            CHECK_FALSE( f == TestEnum::two );
            CHECK_FALSE( f == TestEnum::oneAndTwo );
            CHECK_FALSE( (bool)f );
            CHECK      ( f.none() );
            CHECK_FALSE( f.test(TestEnum::one ));
            CHECK_FALSE( f.test(TestEnum::two ));
        }
    }
}
}

//------------------------------------------------------------------------------
SCENARIO( "Resetting flags", "[Flags]" )
{
GIVEN( "an non-empty Flags instance" )
{
    Flags<TestEnum> f(TestEnum::oneAndTwo);
    WHEN( "a flag is reset" )
    {
        f.reset(TestEnum::one);
        THEN( "that flag tests false" )
        {
            CHECK_FALSE( f.test(TestEnum::one ));
            CHECK      ( f == TestEnum::two );
        }
        AND_WHEN( "another flag is reset" )
        {
            f.reset(TestEnum::two);
            THEN( "both flags test false" )
            {
                CHECK( f.none() );
            }
        }
        AND_WHEN( "a null flag is reset" )
        {
            f.reset(TestEnum::zero);
            THEN( "nothing changes" )
            {
                CHECK( f == TestEnum::two );
            }
        }
        AND_WHEN( "the same flag is reset" )
        {
            f.reset(TestEnum::one);
            THEN( "nothing changes" )
            {
                CHECK( f == TestEnum::two );
            }
        }
    }
    WHEN( "a null flag is set" )
    {
        f.reset(TestEnum::zero);
        THEN( "nothing changes" )
        {
            CHECK( f == TestEnum::oneAndTwo );
        }
    }
    WHEN( "two flags are simulateously reset" )
    {
        f.reset(TestEnum::oneAndTwo);
        THEN( "both flags are reset" )
        {
            CHECK( f.none() );
        }
    }
}
}

//------------------------------------------------------------------------------
SCENARIO( "Flipping flags", "[Flags]" )
{
GIVEN( "an empty Flags instance" )
{
    Flags<TestEnum> f;
    WHEN( "a flag is flipped" )
    {
        f.flip(TestEnum::one);
        CHECK ( f == TestEnum::one );
        AND_WHEN( "more flipping is done" )
        {
            f.flip(TestEnum::two);
            CHECK( f == TestEnum::oneAndTwo );
            f.flip(TestEnum::one);
            CHECK( f == TestEnum::two );
            f.flip(TestEnum::two);
            CHECK( f == TestEnum::zero );
        }
        AND_WHEN( "a null flag is flipped" )
        {
            f.flip(TestEnum::zero);
            THEN( "nothing changes" )
            {
                CHECK( f == TestEnum::one );
            }
        }
    }
    WHEN( "a null flag is flipped" )
    {
        f.flip(TestEnum::zero);
        THEN( "nothing changes" )
        {
            CHECK( f.none() );
        }
    }
}
GIVEN( "one flag set and another reset" )
{
    Flags<TestEnum> f(TestEnum::two);
    WHEN( "flipping both flags simultaneously" )
    {
        f.flip(TestEnum::oneAndTwo);
        THEN( "both flags are flipped" )
        {
            CHECK( f == TestEnum::one );
        }
    }
    WHEN( "a null flag is flipped" )
    {
        f.flip(TestEnum::zero);
        THEN( "nothing changes" )
        {
            CHECK( f == TestEnum::two );
        }
    }
}
}

//------------------------------------------------------------------------------
SCENARIO( "Bitwise-ANDing flags", "[Flags]" )
{
GIVEN( "an non-empty Flags instance" )
{
    Flags<TestEnum> f(TestEnum::oneAndTwo);
    Flags<TestEnum> g = f;

    WHEN( "ANDing with TestEnum::zero" )
    {
        CHECK ( (f &= TestEnum::zero) == TestEnum::zero );
        CHECK ( (g &  TestEnum::zero) == TestEnum::zero );
    }
    WHEN( "ANDing with TestEnum::one" )
    {
        CHECK ( (f &= TestEnum::one) == TestEnum::one );
        CHECK ( (g &  TestEnum::one) == TestEnum::one );
        AND_WHEN( "ANDing with TestEnum::two" )
        {
            CHECK ( (f &= TestEnum::two) == TestEnum::zero );
            CHECK ( (g &  TestEnum::two) == TestEnum::two );
        }
    }
    WHEN( "ANDing with TestEnum::two" )
    {
        CHECK ( (f &= TestEnum::two) == TestEnum::two );
        CHECK ( (g &  TestEnum::two) == TestEnum::two );
        AND_WHEN( "ANDing with TestEnum::one" )
        {
            CHECK ( (f &= TestEnum::one) == TestEnum::zero );
            CHECK ( (g &  TestEnum::one) == TestEnum::one );
        }
    }
    WHEN( "ANDing with TestEnum::oneAndTwo" )
    {
        CHECK ( (f &= TestEnum::oneAndTwo) == TestEnum::oneAndTwo );
        CHECK ( (g &  TestEnum::oneAndTwo) == TestEnum::oneAndTwo );
    }
}
GIVEN( "an empty Flags instance" )
{
    Flags<TestEnum> f;
    Flags<TestEnum> g;

    WHEN( "ANDing with TestEnum::zero" )
    {
        CHECK ( (f &= TestEnum::zero) == TestEnum::zero );
        CHECK ( (g &  TestEnum::zero) == TestEnum::zero );
    }
    WHEN( "ANDing with TestEnum::one" )
    {
        CHECK ( (f &= TestEnum::one) == TestEnum::zero );
        CHECK ( (g &  TestEnum::one) == TestEnum::zero );
    }
    WHEN( "ANDing with TestEnum::two" )
    {
        CHECK ( (f &= TestEnum::two) == TestEnum::zero );
        CHECK ( (g &  TestEnum::two) == TestEnum::zero );
    }
    WHEN( "ANDing with TestEnum::oneAndTwo" )
    {
        CHECK ( (f &= TestEnum::oneAndTwo) == TestEnum::zero );
        CHECK ( (g &  TestEnum::oneAndTwo) == TestEnum::zero );
    }
}
}

//------------------------------------------------------------------------------
SCENARIO( "Bitwise-ORing flags", "[Flags]" )
{
GIVEN( "an empty Flags instance" )
{
    Flags<TestEnum> f(TestEnum::zero);
    Flags<TestEnum> g = f;

    WHEN( "ORing with TestEnum::zero" )
    {
        CHECK ( (f |= TestEnum::zero) == TestEnum::zero );
        CHECK ( (g |  TestEnum::zero) == TestEnum::zero );
    }
    WHEN( "ORing with TestEnum::one" )
    {
        CHECK ( (f |= TestEnum::one) == TestEnum::one );
        CHECK ( (g |  TestEnum::one) == TestEnum::one );
        AND_WHEN( "ORing with TestEnum::two" )
        {
            CHECK ( (f |= TestEnum::two) == TestEnum::oneAndTwo );
            CHECK ( (g |  TestEnum::two) == TestEnum::two );
        }
    }
    WHEN( "ORing with TestEnum::two" )
    {
        CHECK ( (f |= TestEnum::two) == TestEnum::two );
        CHECK ( (g |  TestEnum::two) == TestEnum::two );
        AND_WHEN( "ORing with TestEnum::one" )
        {
            CHECK ( (f |= TestEnum::one) == TestEnum::oneAndTwo );
            CHECK ( (g |  TestEnum::one) == TestEnum::one );
        }
    }
    WHEN( "ORing with TestEnum::oneAndTwo" )
    {
        CHECK ( (f |= TestEnum::oneAndTwo) == TestEnum::oneAndTwo );
        CHECK ( (g |  TestEnum::oneAndTwo) == TestEnum::oneAndTwo );
    }
}
GIVEN( "a non-empty Flags instance" )
{
    Flags<TestEnum> f(TestEnum::oneAndTwo);
    Flags<TestEnum> g = f;

    WHEN( "ORing with TestEnum::zero" )
    {
        CHECK ( (f |= TestEnum::zero) == TestEnum::oneAndTwo );
        CHECK ( (g |  TestEnum::zero) == TestEnum::oneAndTwo );
    }
    WHEN( "ORing with TestEnum::one" )
    {
        CHECK ( (f |= TestEnum::one) == TestEnum::oneAndTwo );
        CHECK ( (g |  TestEnum::one) == TestEnum::oneAndTwo );
    }
    WHEN( "ORing with TestEnum::two" )
    {
        CHECK ( (f |= TestEnum::two) == TestEnum::oneAndTwo );
        CHECK ( (g |  TestEnum::two) == TestEnum::oneAndTwo );
    }
    WHEN( "ORing with TestEnum::oneAndTwo" )
    {
        CHECK ( (f |= TestEnum::oneAndTwo) == TestEnum::oneAndTwo );
        CHECK ( (g |  TestEnum::oneAndTwo) == TestEnum::oneAndTwo );
    }
}
}

//------------------------------------------------------------------------------
SCENARIO( "Bitwise-XORing flags", "[Flags]" )
{
GIVEN( "an empty Flags instance" )
{
    Flags<TestEnum> f;
    Flags<TestEnum> g;

    WHEN( "XORing with TestEnum::zero" )
    {
        CHECK ( (f ^= TestEnum::zero) == TestEnum::zero );
        CHECK ( (g ^  TestEnum::zero) == TestEnum::zero );
    }
    WHEN( "XORing with TestEnum::one" )
    {
        CHECK ( (f ^= TestEnum::one) == TestEnum::one );
        CHECK ( (g ^  TestEnum::one) == TestEnum::one );
        AND_WHEN( "XORing with TestEnum::two" )
        {
            CHECK ( (f ^= TestEnum::two) == TestEnum::oneAndTwo );
            CHECK ( (g ^  TestEnum::two) == TestEnum::two );
        }
    }
    WHEN( "XORing with TestEnum::two" )
    {
        CHECK ( (f ^= TestEnum::two) == TestEnum::two );
        CHECK ( (g ^  TestEnum::two) == TestEnum::two );
        AND_WHEN( "XORing with TestEnum::one" )
        {
            CHECK ( (f ^= TestEnum::one) == TestEnum::oneAndTwo );
            CHECK ( (g ^  TestEnum::one) == TestEnum::one );
        }
    }
    WHEN( "XORing with TestEnum::oneAndTwo" )
    {
        CHECK ( (f ^= TestEnum::oneAndTwo) == TestEnum::oneAndTwo );
        CHECK ( (g ^  TestEnum::oneAndTwo) == TestEnum::oneAndTwo );
    }
}
GIVEN( "a non-empty Flags instance" )
{
    Flags<TestEnum> f(TestEnum::oneAndTwo);
    Flags<TestEnum> g = f;

    WHEN( "XORing with TestEnum::zero" )
    {
        CHECK ( (f ^= TestEnum::zero) == TestEnum::oneAndTwo );
        CHECK ( (g ^  TestEnum::zero) == TestEnum::oneAndTwo );
    }
    WHEN( "XORing with TestEnum::one" )
    {
        CHECK ( (f ^= TestEnum::one) == TestEnum::two );
        CHECK ( (g ^  TestEnum::one) == TestEnum::two );
        AND_WHEN( "XORing with TestEnum::two" )
        {
            CHECK ( (f ^= TestEnum::two) == TestEnum::zero );
            CHECK ( (g ^  TestEnum::two) == TestEnum::one );
        }
    }
    WHEN( "XORing with TestEnum::two" )
    {
        CHECK ( (f ^= TestEnum::two) == TestEnum::one );
        CHECK ( (g ^  TestEnum::two) == TestEnum::one );
        AND_WHEN( "XORing with TestEnum::one" )
        {
            CHECK ( (f ^= TestEnum::one) == TestEnum::zero );
            CHECK ( (g ^  TestEnum::one) == TestEnum::two );
        }
    }
    WHEN( "XORing with TestEnum::oneAndTwo" )
    {
        CHECK ( (f ^= TestEnum::oneAndTwo) == TestEnum::zero );
        CHECK ( (g ^  TestEnum::oneAndTwo) == TestEnum::zero );
    }
}
}

//------------------------------------------------------------------------------
SCENARIO( "Bitwise-inverting flags", "[Flags]" )
{
GIVEN( "an empty Flags instance" )
{
    Flags<TestEnum> f;
    Flags<TestEnum> g;

    WHEN( "inverting" )
    {
        CHECK ( (~f).value() == ~(g.value()) );
    }
}
GIVEN( "a non-empty Flags instance" )
{
    Flags<TestEnum> f(TestEnum::oneAndTwo);
    Flags<TestEnum> g = f;

    WHEN( "inverting" )
    {
        CHECK ( (~f).value() == ~(g.value()) );
    }
}
}

//------------------------------------------------------------------------------
SCENARIO( "Bitwise operators with enumerators on the left-hand side",
          "[Flags]" )
{
auto a = TestEnum::one;
auto b = TestEnum::two;
Flags<TestEnum> g = b;

WHEN( "ANDing" )
{
    auto f = a & b;
    CHECK(typeid(decltype(f)) == typeid(Flags<TestEnum>));
    CHECK(f == TestEnum::zero);
    f = a & g;
    CHECK(f == TestEnum::zero);
}

WHEN( "ORing" )
{
    auto f = a | b;
    CHECK(typeid(decltype(f)) == typeid(Flags<TestEnum>));
    CHECK(f == TestEnum::oneAndTwo);
    f = a | g;
    CHECK(f == TestEnum::oneAndTwo);
}

WHEN( "XORing" )
{
    auto f = a ^ b;
    CHECK(typeid(decltype(f)) == typeid(Flags<TestEnum>));
    CHECK(f == TestEnum::oneAndTwo);
    f = a ^ TestEnum::oneAndTwo;
    CHECK(f == TestEnum::two);
    f = a ^ g;
    CHECK(f == TestEnum::oneAndTwo);
    f = a ^ Flags<TestEnum>(TestEnum::oneAndTwo);
    CHECK(f == TestEnum::two);
}

WHEN( "Inverting" )
{
    auto f = ~a;
    CHECK(typeid(decltype(f)) == typeid(Flags<TestEnum>));
    CHECK(f.value() == ~(Flags<TestEnum>(a).value()));
}
}

//------------------------------------------------------------------------------
SCENARIO( "Comparisons with enumerators on the left-hand side", "[Flags]" )
{
auto a = TestEnum::one;
Flags<TestEnum> f = TestEnum::one;
Flags<TestEnum> g = TestEnum::two;

WHEN( "comparing" )
{
    CHECK(a == f);
    CHECK(a != g);
    CHECK_FALSE(a != f);
    CHECK_FALSE(a == g);
}
}

//------------------------------------------------------------------------------
SCENARIO( "constexpr Flags", "[Flags]" )
{
    using F = Flags<TestEnum>;
    using E = TestEnum;
    static_assert(F().none(), "");
    static_assert(F(E::one).value() == unsigned(E::one), "");
    static_assert(F(E::one).value() != unsigned(E::two), "");
    static_assert(F(E::one) == F(E::one), "");
    static_assert(F(E::one) != F(E::two), "");
    static_assert(F(E::one).test(E::one), "");
    static_assert(F(E::one).any(E::one), "");
    static_assert((F(E::one) & F(E::oneAndTwo)) == F(E::one), "");
    static_assert((F(E::one) | F(E::two)) == F(E::oneAndTwo), "");
    static_assert((F(E::oneAndTwo) ^ F(E::two)) == F(E::one), "");
    static_assert((~F(E::one)).value() == ~(unsigned(E::one)), "");
    static_assert((E::oneAndTwo & E::one) == F(E::one), "");
    static_assert((E::one | E::two) == F(E::oneAndTwo), "");
    static_assert((E::oneAndTwo ^ E::one) == F(E::two), "");
    static_assert(~E::one == F(in_place, ~(unsigned(E::one))), "");
    static_assert(E::one == F(E::one), "");
    static_assert(E::one != F(E::two), "");
}

//------------------------------------------------------------------------------
#ifdef CPPWAMP_HAS_RELAXED_CONSTEXPR
SCENARIO( "Relaxed constexpr Flags", "[Flags]" )
{
    using F = Flags<TestEnum>;
    using E = TestEnum;

    struct Checker
    {
        static constexpr bool check()
        {
            F f{E::one};
            F g{E::two};
            f.reset(E::one);
            f.set(E::one);
            f.flip(E::one);
            f &= g;
            f |= g;
            f ^= g;
            return true;
        }
    };

    static_assert(Checker::check(), "");
}
#endif
