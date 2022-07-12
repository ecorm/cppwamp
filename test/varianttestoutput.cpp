/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#include <cstdlib>
#include <limits>
#include <sstream>
#include <type_traits>
#include <catch2/catch.hpp>
#include <cppwamp/variant.hpp>

using namespace wamp;

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

} // anonymous namespace

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
