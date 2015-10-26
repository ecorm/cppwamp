/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#if CPPWAMP_TESTING_VARIANT

#include <cstdlib>
#include <limits>
#include <type_traits>
#include <catch.hpp>
#include <cppwamp/variant.hpp>

using namespace wamp;

namespace
{

//------------------------------------------------------------------------------
template <typename TLower, typename TGreater>
bool differs(const TLower& lower, const TGreater& greater)
{
    INFO( "with lower=" << lower << " greater=" << greater );

    using V = Variant;
    bool b[18] = {false};
    int i = 0;
    CHECK(( b[i++]= !(V(lower)   != V(lower)) ));
    CHECK(( b[i++]= !(V(lower)   <  V(lower)) ));
    CHECK(( b[i++]= !(V(lower)   == V(greater)) ));
    CHECK(( b[i++]=   V(lower)   != V(greater) ));
    CHECK(( b[i++]=   V(lower)   <  V(greater) ));
    CHECK(( b[i++]= !(V(greater) == V(lower)) ));
    CHECK(( b[i++]=   V(greater) != V(lower) ));
    CHECK(( b[i++]= !(V(greater) <  V(lower)) ));

    CHECK(( b[i++]= !(V(lower)   != lower) ));
    CHECK(( b[i++]= !(V(lower)   == greater) ));
    CHECK(( b[i++]=   V(lower)   != greater ));
    CHECK(( b[i++]= !(V(greater) == lower) ));
    CHECK(( b[i++]=   V(greater) != lower ));

    CHECK(( b[i++]= !(lower   != V(lower)) ));
    CHECK(( b[i++]= !(lower   == V(greater)) ));
    CHECK(( b[i++]=   lower   != V(greater) ));
    CHECK(( b[i++]= !(greater == V(lower)) ));
    CHECK(( b[i++]=   greater != V(lower) ));

    for (auto result: b)
        if (!result)
            return false;
    return true;
}

//------------------------------------------------------------------------------
template <typename TLeft, typename TRight>
bool same(const TLeft& lhs, const TRight& rhs)
{
    INFO( "with lhs=" << lhs << " rhs=" << rhs );

    using V = Variant;
    bool b[14] = {false};
    int i = 0;

    CHECK(( b[i++]= !(V(lhs) != V(rhs)) ));
    CHECK(( b[i++]=   V(lhs) == V(rhs) ));
    CHECK(( b[i++]= !(V(lhs) <  V(rhs)) ));
    CHECK(( b[i++]= !(V(rhs) != V(lhs)) ));
    CHECK(( b[i++]=   V(rhs) == V(lhs) ));
    CHECK(( b[i++]= !(V(rhs) <  V(lhs)) ));

    CHECK(( b[i++]= !(lhs != V(rhs)) ));
    CHECK(( b[i++]=   lhs == V(rhs) ));
    CHECK(( b[i++]= !(rhs != V(lhs)) ));
    CHECK(( b[i++]=   rhs == V(lhs) ));

    CHECK(( b[i++]= !(V(lhs) != rhs) ));
    CHECK(( b[i++]=   V(lhs) == rhs ));
    CHECK(( b[i++]= !(V(rhs) != lhs) ));
    CHECK(( b[i++]=   V(rhs) == lhs ));

    for (auto result: b)
        if (!result)
            return false;
    return true;
}

} // anonymous namespace

//------------------------------------------------------------------------------
SCENARIO( "Variant comparisons", "[Variant]" )
{
auto intMin  = std::numeric_limits<Int>::min();
auto intMax  = std::numeric_limits<Int>::max();
auto uintMax = std::numeric_limits<UInt>::max();
auto realMin = std::numeric_limits<Real>::lowest();
auto realMax = std::numeric_limits<Real>::max();
using Ar = Array;
using Ob = Object;

GIVEN( "Variants of the same dynamic type" )
{
    WHEN( "using Bool Variants" )
    {
        CHECK( differs(false, true) );
    }
    WHEN( "using Int Variants" )
    {
        CHECK( differs(0, 1) );
        CHECK( differs(-1, 0) );
        CHECK( differs(intMin, 0) );
        CHECK( differs(0, intMax) );
        CHECK( differs(intMin, intMax) );
    }
    WHEN( "using UInt Variants" )
    {
        CHECK( differs(0u, 1u) );
        CHECK( differs(0u, uintMax) );
    }
    WHEN( "using Real Variants" )
    {
        CHECK( differs(0.0, 1.0) );
        CHECK( differs(-1.0, 0.0) );
        CHECK( differs(realMin, 0.0) );
        CHECK( differs(0.0, realMax) );
        CHECK( differs(realMin, realMax) );
    }
    WHEN( "using String Variants" )
    {
        CHECK( differs("", "A") );
        CHECK( differs("A", "AA") );
        CHECK( differs("A", "B") );
        CHECK( differs("A", "a") );
        CHECK( differs("B", "a") );
    }
    WHEN( "using Blob Variants" )
    {
        WHEN( "the left side is an empty Blob" )
        {
            CHECK( differs(Blob{}, Blob{0x00}) );
            CHECK( differs(Blob{}, Blob{0x00, 0x01, 0x02}) );
        }
        WHEN ( "performing lexicographical comparisons" )
        {
            CHECK( differs(Blob{0x00}, Blob{0x01}) );
            CHECK( differs(Blob{0x00}, Blob{0x00, 0x00}) );
            CHECK( differs(Blob{0x01}, Blob{0x01, 0x00}) );
            CHECK( differs(Blob{0x01}, Blob{0x01, 0x01}) );
            CHECK( differs(Blob{0x00, 0x00}, Blob{0x01}) );
        }
    }
    WHEN( "using Array Variants" )
    {
        WHEN( "the left side is an empty Array" )
        {
            CHECK( differs(Ar{},   Ar{null}) );
            CHECK( differs(Ar{},   Ar{false}) );
            CHECK( differs(Ar{},   Ar{0}) );
            CHECK( differs(Ar{},   Ar{0u}) );
            CHECK( differs(Ar{},   Ar{0.0}) );
            CHECK( differs(Ar{},   Ar{""}) );
            CHECK( differs(Ar{},   Ar{Ar{}}) );
            CHECK( differs(Ar{},   Ar{Ob{}}) );
        }
        WHEN ( "performing lexicographical comparisons" )
        {
            CHECK( differs(Ar{0},     Ar{1}) );
            CHECK( differs(Ar{-1},    Ar{0}) );
            CHECK( differs(Ar{0},     Ar{0,0}) );
            CHECK( differs(Ar{1},     Ar{1,0}) );
            CHECK( differs(Ar{1},     Ar{1,1}) );
            CHECK( differs(Ar{0,0},   Ar{1}) );
            CHECK( differs(Ar{0,0},   Ar{0,1}) );
            CHECK( differs(Ar{0,0},   Ar{1,0}) );
            CHECK( differs(Ar{0,0},   Ar{1,1}) );
            CHECK( differs(Ar{0,1},   Ar{1,0}) );
            CHECK( differs(Ar{0,1},   Ar{1,1}) );
            CHECK( differs(Ar{1,0},   Ar{1,1}) );
            CHECK( differs(Ar{1,0,0}, Ar{1,1}) );
        }
    }
    WHEN( "using Object Variants" )
    {
        WHEN( "the left side is an empty Object" )
        {
            CHECK( differs(Ob{},   Ob{{"", null}}) );
            CHECK( differs(Ob{},   Ob{{"", false}}) );
            CHECK( differs(Ob{},   Ob{{"", 0}}) );
            CHECK( differs(Ob{},   Ob{{"", 0u}}) );
            CHECK( differs(Ob{},   Ob{{"", 0.0}}) );
            CHECK( differs(Ob{},   Ob{{"", ""}}) );
            CHECK( differs(Ob{},   Ob{{"", Ob{}}}) );
            CHECK( differs(Ob{},   Ob{{"", Ob{}}}) );
        }
        WHEN( "both Objects have a single, identical key" )
        {
            CHECK( differs(Ob{ {"k", false} }, Ob{ {"k", true} }) );
            CHECK( differs(Ob{ {"k", -1} },    Ob{ {"k", 0} }) );
            CHECK( differs(Ob{ {"k", 0u} },    Ob{ {"k", 1u} }) );
            CHECK( differs(Ob{ {"k", 0.0} },   Ob{ {"k", 1.0} }) );
            CHECK( differs(Ob{ {"k", "A"} },   Ob{ {"k", "B"} }) );
            CHECK( differs(Ob{ {"k", Ar{}}},   Ob{ {"k", Ar{null}}}) );
            CHECK( differs(Ob{ {"k", Ob{}} },  Ob{ {"k", Ob{{"",null}}} }) );
        }
        WHEN ( "performing lexicographical comparisons on only the key" )
        {
            CHECK( differs(Ob{ {"A", null} },  Ob{ {"AA", null} }) );
            CHECK( differs(Ob{ {"A", null} },  Ob{ {"B",  null} }) );
            CHECK( differs(Ob{ {"A", null} },  Ob{ {"a",  null} }) );
            CHECK( differs(Ob{ {"B", null} },  Ob{ {"BA", null} }) );
            CHECK( differs(Ob{ {"B", null} },  Ob{ {"a",  null} }) );
        }
        WHEN ( "performing lexicographical comparisons on both key and value" )
        {
            CHECK( differs(Ob{ {"A", true} },          Ob{ {"AA", false} }) );
            CHECK( differs(Ob{ {"A", 0} },             Ob{ {"B",  -1} }) );
            CHECK( differs(Ob{ {"A", "a"} },           Ob{ {"a", "A"} }) );
            CHECK( differs(Ob{ {"B", Ar{null}} },      Ob{ {"BA", Ar{}} }) );
            CHECK( differs(Ob{ {"B", Ob{{"",null}}} }, Ob{ {"a", Ob{}} }) );
        }
        WHEN ( "object member count differs" )
        {
            CHECK( differs(Ob{ {"A", null} },             Ob{ {"A", null}, {"B", null} }) );
            CHECK( differs(Ob{ {"A", null}, {"B", null}}, Ob{ {"B", null} } ) );
            CHECK( differs(Ob{ {"A", 1} },                Ob{ {"B", 0}, {"C", 0} }) );
            CHECK( differs(Ob{ {"A", 42.0}, {"B", 42.0}}, Ob{ {"B", -42.0} } ) );
        }
    }
}

GIVEN( "Two variants of numeric type (integer or real)" )
{
    // Type ordering is: null, boolean, number, string, array, object.
    CHECK( same   (0,   0u) );
    CHECK( same   (0,   0.0) );
    CHECK( same   (0u,  0.0) );
    CHECK( same   (-1,  -1.0) );
    CHECK( differs(0,   1u) );
    CHECK( differs(0,   1.0) );
    CHECK( differs(0,   0.1) );
    CHECK( differs(-1,  0) );
    CHECK( differs(0u,  -1) ); // Signed/unsigned comparison
    CHECK( differs(-1,  0.0) );
    CHECK( differs(-1,  -0.9) );
    CHECK( differs(0u,  1) );
    CHECK( differs(0u,  1.0) );
    CHECK( differs(0u,  0.1) );
    CHECK( differs(0.0, 1) );
    CHECK( differs(0.0, 1u) );

    CHECK( same   (Ar{0},   Ar{0u}) );
    CHECK( same   (Ar{0},   Ar{0.0}) );
    CHECK( same   (Ar{0u},  Ar{0.0}) );
    CHECK( same   (Ar{-1},  Ar{-1.0}) );
    CHECK( differs(Ar{0},   Ar{1u}) );
    CHECK( differs(Ar{0},   Ar{1.0}) );
    CHECK( differs(Ar{0},   Ar{0.1}) );
    CHECK( differs(Ar{-1},  Ar{0}) );
    CHECK( differs(Ar{0u},  Ar{-1}) ); // Signed/unsigned comparison
    CHECK( differs(Ar{-1},  Ar{0.0}) );
    CHECK( differs(Ar{-1},  Ar{-0.9}) );
    CHECK( differs(Ar{0u},  Ar{1}) );
    CHECK( differs(Ar{0u},  Ar{1.0}) );
    CHECK( differs(Ar{0u},  Ar{0.1}) );
    CHECK( differs(Ar{0.0}, Ar{1}) );
    CHECK( differs(Ar{0.0}, Ar{1u}) );

    CHECK( same   (Ob{{"a",0}},   Ob{{"a",0u}}) );
    CHECK( same   (Ob{{"a",0}},   Ob{{"a",0.0}}) );
    CHECK( same   (Ob{{"a",0u}},  Ob{{"a",0.0}}) );
    CHECK( same   (Ob{{"a",-1}},  Ob{{"a",-1.0}}) );
    CHECK( differs(Ob{{"a",0}},   Ob{{"a",1u}}) );
    CHECK( differs(Ob{{"a",0}},   Ob{{"a",1.0}}) );
    CHECK( differs(Ob{{"a",0}},   Ob{{"a",0.1}}) );
    CHECK( differs(Ob{{"a",-1}},  Ob{{"a",0}}) );
    CHECK( differs(Ob{{"a",0u}},  Ob{{"a",-1}}) ); // Signed/unsigned comparison
    CHECK( differs(Ob{{"a",-1}},  Ob{{"a",0.0}}) );
    CHECK( differs(Ob{{"a",-1}},  Ob{{"a",-0.9}}) );
    CHECK( differs(Ob{{"a",0u}},  Ob{{"a",1}}) );
    CHECK( differs(Ob{{"a",0u}},  Ob{{"a",1.0}}) );
    CHECK( differs(Ob{{"a",0u}},  Ob{{"a",0.1}}) );
    CHECK( differs(Ob{{"a",0.0}}, Ob{{"a",1}}) );
    CHECK( differs(Ob{{"a",0.0}}, Ob{{"a",1u}}) );
}

GIVEN( "Variants of different dynamic types" )
{
    // Type ordering is: null, boolean, number, string, array, object.
    CHECK( differs(null,        false) );
    CHECK( differs(null,        true) );

    CHECK( differs(false,       0) );
    CHECK( differs(false,       0u) );
    CHECK( differs(false,       0.0) );
    CHECK( differs(false,       intMin) );
    CHECK( differs(false,       realMin) );
    CHECK( differs(true,        0) );
    CHECK( differs(true,        0u) );
    CHECK( differs(true,        0.0) );
    CHECK( differs(true,        1) );
    CHECK( differs(true,        1u) );
    CHECK( differs(true,        1.0) );
    CHECK( differs(true,        intMin) );
    CHECK( differs(true,        realMin) );

    CHECK( differs(0,           "") );
    CHECK( differs(0u,          "") );
    CHECK( differs(0.0,         "") );
    CHECK( differs(intMax,      "") );
    CHECK( differs(uintMax,     "") );
    CHECK( differs(realMax,     "") );

    CHECK( differs("",          Array{}) );
    CHECK( differs("Z",         Array{}) );
    CHECK( differs("A",         Array{"A"}) );
    CHECK( differs("Z",         Array{"A"}) );

    CHECK( differs("",          Blob{}) );
    CHECK( differs("Z",         Blob{}) );
    CHECK( differs("A",         Blob{'A'}) );
    CHECK( differs("Z",         Blob{'Z'}) );

    CHECK( differs(Blob{},      Array{}) );
    CHECK( differs(Blob{0x00},  Array{0}) );

    CHECK( differs(Array{},     Object{}) );
    CHECK( differs(Array{"Z"},  Object{}) );
    CHECK( differs(Array{"Z"},  Object{{"A",0}}) );
}
}

#endif // #if CPPWAMP_TESTING_VARIANT
