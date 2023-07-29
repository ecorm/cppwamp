/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include <cstdlib>
#include <limits>
#include <sstream>
#include <type_traits>
#include <catch2/catch.hpp>
#include <cppwamp/variant.hpp>

using namespace wamp;
namespace Matchers = Catch::Matchers;

namespace
{

//------------------------------------------------------------------------------
template <typename T>
void checkOutput(const T& value, const std::string& expected)
{
    Variant v(value);
    INFO( "For variant of type '" << typeNameOf(v) <<
          "' and value '" << value << "'" );

    std::ostringstream out;
    out << v;
    CHECK( out.str() == expected );

    std::string str = toString(v);
    CHECK( str == expected );
}

//------------------------------------------------------------------------------
void checkArray(const Array& array, const std::string& expected)
{
    INFO( "For an Array" );

    std::ostringstream out;
    out << array;
    CHECK( out.str() == expected );

    std::string str = toString(array);
    CHECK( str == expected );

    checkOutput(Variant(array), expected);
}

//------------------------------------------------------------------------------
void checkObject(const Object& object, const std::string& expected)
{
    INFO( "For an Object" );

    std::ostringstream out;
    out << object;
    CHECK( out.str() == expected );

    std::string str = toString(object);
    CHECK( str == expected );

    checkOutput(Variant(object), expected);
}

//------------------------------------------------------------------------------
void checkRealOutput(const Real& value)
{
    Variant v(value);
    INFO( "For variant of type '" << typeNameOf(v) <<
          "' and value '" << value << "'" );

    std::stringstream out;
    out << v;
    Real x;
    out >> x;
    CHECK( x == Approx(value) );

    std::string str = toString(v);
    x = std::stod(str);
    CHECK( x == Approx(value) );
}

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
SCENARIO( "Variant type information", "[Variant]" )
{
    using K = VariantKind;
    GIVEN( "a default-constructed Variant" )
    {
        Variant v;
        THEN( "The type information is as expected" )
        {
            CHECK( (v.kind() == K::null) );
            CHECK_THAT( typeNameOf(v), Matchers::Equals("Null") );
            CHECK( !v );
            CHECK( v.is<Null>() );
            CHECK( v.is<K::null>() );
            CHECK( v.size() == 0 );
            CHECK( !isNumber(v) );
            CHECK( !isScalar(v) );
        }
    }
    GIVEN( "a Null Variant" )
    {
        Variant v(null);
        THEN( "The type information is as expected" )
        {
            CHECK( (v.kind() == K::null) );
            CHECK_THAT( typeNameOf(v), Matchers::Equals("Null") );
            CHECK( !v );
            CHECK( v.is<Null>() );
            CHECK( v.is<K::null>() );
            CHECK( v.size() == 0 );
            CHECK( !isNumber(v) );
            CHECK( !isScalar(v) );
        }
    }
    GIVEN( "a Bool Variant" )
    {
        Variant v(true);
        THEN( "The type information is as expected" )
        {
            CHECK( (v.kind() == K::boolean) );
            CHECK_THAT( typeNameOf(v), Matchers::Equals("Bool") );
            CHECK( !!v );
            CHECK( v.is<Bool>() );
            CHECK( v.is<K::boolean>() );
            CHECK( v.size() == 1 );
            CHECK( !isNumber(v) );
            CHECK( isScalar(v) );
        }
    }
    GIVEN( "an Int Variant" )
    {
        Variant v(Int(-42));
        THEN( "The type information is as expected" )
        {
            CHECK( (v.kind() == K::integer) );
            CHECK_THAT( typeNameOf(v), Matchers::Equals("Int") );
            CHECK( !!v );
            CHECK( v.is<Int>() );
            CHECK( v.is<K::integer>() );
            CHECK( v.size() == 1 );
            CHECK( isNumber(v) );
            CHECK( isScalar(v) );
        }
    }
    GIVEN( "an UInt Variant" )
    {
        Variant v(UInt(42u));
        THEN( "The type information is as expected" )
        {
            CHECK( (v.kind() == K::uint) );
            CHECK_THAT( typeNameOf(v), Matchers::Equals("UInt") );
            CHECK( !!v );
            CHECK( v.is<UInt>() );
            CHECK( v.is<K::uint>() );
            CHECK( v.size() == 1 );
            CHECK( isNumber(v) );
            CHECK( isScalar(v) );
        }
    }
    GIVEN( "a Real Variant" )
    {
        Variant v(Real(42.0));
        THEN( "The type information is as expected" )
        {
            CHECK( (v.kind() == K::real) );
            CHECK_THAT( typeNameOf(v), Matchers::Equals("Real") );
            CHECK( !!v );
            CHECK( v.is<Real>() );
            CHECK( v.is<K::real>() );
            CHECK( v.size() == 1 );
            CHECK( isNumber(v) );
            CHECK( isScalar(v) );
        }
    }
    GIVEN( "a String Variant" )
    {
        Variant v(String("Hello"));
        THEN( "The type information is as expected" )
        {
            CHECK( (v.kind() == K::string) );
            CHECK_THAT( typeNameOf(v), Matchers::Equals("String") );
            CHECK( !!v );
            CHECK( v.is<String>() );
            CHECK( v.is<K::string>() );
            CHECK( v.size() == 1 );
            CHECK( !isNumber(v) );
            CHECK( !isScalar(v) );
        }
    }
    GIVEN( "a Blob Variant" )
    {
        Variant v(Blob{0x00, 0x01, 0x02});
        THEN( "The type information is as expected" )
        {
            CHECK( (v.kind() == K::blob) );
            CHECK_THAT( typeNameOf(v), Matchers::Equals("Blob") );
            CHECK( !!v );
            CHECK( v.is<Blob>() );
            CHECK( v.is<K::blob>() );
            CHECK( v.size() == 1 );
            CHECK( !isNumber(v) );
            CHECK( !isScalar(v) );
        }
    }
    GIVEN( "an Array Variant" )
    {
        Variant v(Array{42, "hello", false});
        THEN( "The type information is as expected" )
        {
            CHECK( (v.kind() == K::array) );
            CHECK_THAT( typeNameOf(v), Matchers::Equals("Array") );
            CHECK( !!v );
            CHECK( v.is<Array>() );
            CHECK( v.is<K::array>() );
            CHECK( v.size() == 3 );
            CHECK( !isNumber(v) );
            CHECK( !isScalar(v) );
        }
    }
    GIVEN( "an Object Variant" )
    {
        Variant v(Object{{"foo",42}, {"bar","hello"}});
        THEN( "The type information is as expected" )
        {
            CHECK( (v.kind() == K::object) );
            CHECK_THAT( typeNameOf(v), Matchers::Equals("Object") );
            CHECK( !!v );
            CHECK( v.is<Object>() );
            CHECK( v.is<K::object>() );
            CHECK( v.size() == 2 );
            CHECK( !isNumber(v) );
            CHECK( !isScalar(v) );
        }
    }
}


//------------------------------------------------------------------------------
SCENARIO( "Variant stream output", "[Variant]" )
{
auto intMin  = std::numeric_limits<Int>::min();
auto intMax  = std::numeric_limits<Int>::max();
auto uintMax = std::numeric_limits<UInt>::max();
auto realMin = std::numeric_limits<Real>::lowest();
auto realMax = std::numeric_limits<Real>::max();

GIVEN( "an assortment of variants" )
{
    checkOutput(null, "null");
    checkOutput(false, "false");
    checkOutput(true, "true");
    checkOutput(0, "0");
    checkOutput(-1, "-1");
    checkOutput(intMin, "-9223372036854775808");
    checkOutput(intMax, "9223372036854775807");
    checkOutput(0u, "0");
    checkOutput(uintMax, "18446744073709551615");
    checkRealOutput(0.0);
    checkRealOutput(realMin);
    checkRealOutput(realMax);
    checkOutput("Hello", R"("Hello")");
    checkOutput("",      R"("")");
    checkOutput("null",  R"("null")");
    checkOutput("false", R"("false")");
    checkOutput("true",  R"("true")");
    checkOutput("0",     R"("0")");
    checkOutput("1",     R"("1")");
    checkOutput(Blob{},                       R"("\u0000")");
    checkOutput(Blob{0x00},                   R"("\u0000AA==")");
    checkOutput(Blob{0x00, 0x01},             R"("\u0000AAE=")");
    checkOutput(Blob{0x00, 0x01, 0x02},       R"("\u0000AAEC")");
    checkOutput(Blob{0x00, 0x01, 0x02, 0x03}, R"("\u0000AAECAw==")");
    checkArray(Array{},      "[]");
    checkArray(Array{null},  "[null]");
    checkArray(Array{false}, "[false]");
    checkArray(Array{true},  "[true]");
    checkArray(Array{0u},    "[0]");
    checkArray(Array{-1},    "[-1]");
    checkArray(Array{""},    R"([""])");

    // Array{Array{}} is ambiguous with the move constructor and the
    // constructor taking an initializer list
    checkArray(Array{Variant{Array{}}},  "[[]]");
    checkArray(Array{Object{}}, "[{}]");
    checkArray(Array{null,false,true,42u,-42,"hello",Array{},Object{}},
               R"([null,false,true,42,-42,"hello",[],{}])");
    checkArray(Array{ Variant{Array{Variant{Array{"foo",42}}}},
                      Array{ Object{{"foo",42}} } },
               R"([[["foo",42]],[{"foo":42}]])");

    checkObject(Object{},                   R"({})");
    checkObject(Object{ {"",""} },          R"({"":""})");
    checkObject(Object{ {"n",null} },       R"({"n":null})");
    checkObject(Object{ {"b",false} },      R"({"b":false})");
    checkObject(Object{ {"b",true} },       R"({"b":true})");
    checkObject(Object{ {"n",0u} },         R"({"n":0})");
    checkObject(Object{ {"n",-1} },         R"({"n":-1})");
    checkObject(Object{ {"s","" } },        R"({"s":""})");
    checkObject(Object{ {"a",Array{}} },    R"({"a":[]})");
    checkObject(Object{ {"o",Object{}} },   R"({"o":{}})");
    checkObject(Object{ {"",null}, {"f",false}, {"t",true}, {"u",0u}, {"n",-1},
                        {"s","abc"}, {"a",Array{}}, {"o",Object{}} },
                R"({"":null,"a":[],"f":false,"n":-1,"o":{},"s":"abc","t":true,"u":0})");
    checkObject(Object{ {"a", Object{ {"b", Object{ {"c",42} }} } } },
                R"({"a":{"b":{"c":42}}})");
}
}

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
            CHECK( differs(Ar{},   Ar{Variant{Ar{}}}) );
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
    CHECK( differs(-1,  0u) );
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
    CHECK( differs(Ar{-1},  Ar{0u}) );
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
    CHECK( differs(Ob{{"a",-1}},  Ob{{"a",0u}}) );
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
