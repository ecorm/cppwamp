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
    checkOutput("Hello", "Hello");
    checkOutput("", "");
    checkOutput("null", "null");
    checkOutput("false", "false");
    checkOutput("true", "true");
    checkOutput("0", "0");
    checkOutput("1", "1");
    checkOutput(Blob{}, "");
    checkOutput(Blob{0x00}, "AA==");
    checkOutput(Blob{0x00, 0x01}, "AAE=");
    checkOutput(Blob{0x00, 0x01, 0x02}, "AAEC");
    checkOutput(Blob{0x00, 0x01, 0x02, 0x03}, "AAECAw==");
    checkOutput(Array{},         "[]");
    checkOutput(Array{null},     "[null]");
    checkOutput(Array{false},    "[false]");
    checkOutput(Array{true},     "[true]");
    checkOutput(Array{0u},       "[0]");
    checkOutput(Array{-1},       "[-1]");
    checkOutput(Array{""},       "[\"\"]");

    // Array{Array{}} is ambiguous with the move constructor and the
    // constructor taking an initializer list
    checkOutput(Array{Variant{Array{}}},  "[[]]");
    checkOutput(Array{Object{}}, "[{}]");
    checkOutput(Array{null,false,true,42u,-42,"hello",Array{},Object{}},
                R"([null,false,true,42,-42,"hello",[],{}])");
    checkOutput(Array{ Variant{Array{Variant{Array{"foo",42}}}},
                       Array{ Object{{"foo",42}} } },
                R"([[["foo",42]],[{"foo":42}]])");

    checkOutput(Object{},                   R"({})");
    checkOutput(Object{ {"",""} },          R"({"":""})");
    checkOutput(Object{ {"n",null} },       R"({"n":null})");
    checkOutput(Object{ {"b",false} },      R"({"b":false})");
    checkOutput(Object{ {"b",true} },       R"({"b":true})");
    checkOutput(Object{ {"n",0u} },         R"({"n":0})");
    checkOutput(Object{ {"n",-1} },         R"({"n":-1})");
    checkOutput(Object{ {"s","" } },        R"({"s":""})");
    checkOutput(Object{ {"a",Array{}} },    R"({"a":[]})");
    checkOutput(Object{ {"o",Object{}} },   R"({"o":{}})");
    checkOutput(Object{ {"",null}, {"f",false}, {"t",true}, {"u",0u}, {"n",-1},
                        {"s","abc"}, {"a",Array{}}, {"o",Object{}} },
                R"({"":null,"a":[],"f":false,"n":-1,"o":{},"s":"abc","t":true,"u":0})");
    checkOutput(Object{ {"a", Object{ {"b", Object{ {"c",42} }} } } },
                R"({"a":{"b":{"c":42}}})");
}
}
