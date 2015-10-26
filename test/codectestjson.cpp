/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#if CPPWAMP_TESTING_CODEC

#include <cmath>
#include <limits>
#include <sstream>
#include <catch.hpp>
#include <cppwamp/variant.hpp>
#include <cppwamp/json.hpp>

using namespace wamp;

namespace
{

//------------------------------------------------------------------------------
std::string toString(const Variant& v)
{
    if (!v.is<String>())
        return v.to<String>();
    else
        return '"' + v.to<String>() + '"';
}

//------------------------------------------------------------------------------
template <typename T>
void checkJson(const std::string& json, const T& value,
               const std::string& serialized)
{
    INFO( "For JSON string \"" << json << "\"" );
    Variant expected(value);

    {
        Variant v;
        CHECK_NOTHROW( Json::decode(json, v) );
        CHECK( v == expected );

        std::string str;
        Json::encode(v, str);
        CHECK( v == expected );
        CHECK( str == serialized );

        std::ostringstream oss;
        Json::encode(v, oss);
        CHECK( v == expected );
        CHECK( oss.str() == serialized );
    }

    {
        Variant v;
        std::istringstream iss(json);
        CHECK_NOTHROW( Json::decode(iss, v) );
        CHECK( v == expected );
    }
}

//------------------------------------------------------------------------------
template <typename T>
void checkJson(const std::string& json, const T& value)
{
    checkJson(json, value, json);
}

//------------------------------------------------------------------------------
template <typename TExpected, typename TInteger>
void checkInteger(const std::string& json, TInteger n)
{
    INFO( "For JSON string \"" << json << "\"" );

    {
        Variant v;
        CHECK_NOTHROW( Json::decode(json, v) );
        REQUIRE( v.is<TExpected>() );
        CHECK( v.as<TExpected>() == n );
        CHECK( v == n );
    }

    {
        Variant v;
        std::istringstream iss(json);
        CHECK_NOTHROW( Json::decode(iss, v) );
        REQUIRE( v.is<TExpected>() );
        CHECK( v.as<TExpected>() == n );
        CHECK( v == n );
    }
}

//------------------------------------------------------------------------------
void checkReal(const std::string& json, double x)
{
    INFO( "For JSON string \"" << json << "\"" );
    auto epsilon = std::numeric_limits<Real>::epsilon()*10.0;
    {
        Variant v;
        CHECK_NOTHROW( Json::decode(json, v) );
        REQUIRE( v.is<Real>() );
        CHECK( v.as<Real>() == Approx(x).epsilon(epsilon) );
    }

    {
        Variant v;
        std::istringstream iss(json);
        CHECK_NOTHROW( Json::decode(iss, v) );
        REQUIRE( v.is<Real>() );
        CHECK( v.as<Real>() == Approx(x).epsilon(epsilon) );
    }
}

//------------------------------------------------------------------------------
void checkError(const std::string& json)
{
    INFO( "For JSON string \"" << json << "\"" );

    {
        auto originalValue = Array{null, true, 42, "hello"};
        Variant v(originalValue);
        CHECK_THROWS_AS( Json::decode(json, v), error::Decode );
        CHECK( v == originalValue );
    }
}

} // anonymous namespace

//------------------------------------------------------------------------------
SCENARIO( "JSON serialization", "[Variant]" )
{
GIVEN( "valid JSON numeric strings" )
{
    auto intMin  = std::numeric_limits<Int>::min();
    auto intMax  = std::numeric_limits<Int>::max();
    auto uintMax = std::numeric_limits<UInt>::max();

    checkInteger<Int>("0",   0);
    checkInteger<Int>("1",   1);
    checkInteger<Int>("-1", -1);
    checkInteger<Int>("-9223372036854775808",  intMin);
    checkInteger<Int>("9223372036854775807",   intMax);
    checkInteger<UInt>("9223372036854775808",  9223372036854775808ull);
    checkInteger<UInt>("18446744073709551615", uintMax);

    checkReal("0.0", 0.0);
    checkReal("1.0", 1.0);
    checkReal("-1.0", -1.0);
    checkReal("3.14159265358979324", 3.14159265358979324);
    checkReal("2.9979e8", 2.9979e8);
}
GIVEN( "valid JSON strings" )
{
    auto intMin  = std::numeric_limits<Int>::min();
    auto intMax  = std::numeric_limits<Int>::max();
    auto uintMax = std::numeric_limits<UInt>::max();

    checkJson(R"(null)",    null);
    checkJson(R"(false)",   false);
    checkJson(R"(true)",    true);
    checkJson(R"("")", "");
    checkJson(R"("Hello")", "Hello");
    checkJson(R"("null")",  "null");
    checkJson(R"("false")", "false");
    checkJson(R"("true")",  "true");
    checkJson(R"("0")",     "0");
    checkJson(R"("1")",     "1");
    checkJson(R"("\u0000")",         Blob{});
    checkJson(R"("\u0000AA==")",     Blob{0x00});
    checkJson(R"("\u0000Zg==")",     Blob{'f'});
    checkJson(R"("\u0000Zm8=")",     Blob{'f','o'});
    checkJson(R"("\u0000Zm9v")",     Blob{'f','o','o'});
    checkJson(R"("\u0000Zm9vYg==")", Blob{'f','o','o','b'});
    checkJson(R"("\u0000Zm9vYmE=")", Blob{'f','o','o','b','a'});
    checkJson(R"("\u0000Zm9vYmFy")", Blob{'f','o','o','b','a','r'});
    checkJson(R"("\u0000FPucAw==")", Blob{0x14, 0xfb, 0x9c, 0x03});
    checkJson(R"("\u0000FPucA9k=")", Blob{0x14, 0xfb, 0x9c, 0x03, 0xd9});
    checkJson(R"("\u0000FPucA9l+")", Blob{0x14, 0xfb, 0x9c, 0x03, 0xd9, 0x7e});
    checkJson(R"([])",      Array{});
    checkJson(R"([null])",  Array{null});
    checkJson(R"([false])", Array{false});
    checkJson(R"([true])",  Array{true});
    checkJson(R"([0])",     Array{0u});
    checkJson(R"([-1])",    Array{-1});
    checkJson(R"([9223372036854775807])", Array{(UInt)intMax});
    checkJson(R"([9223372036854775808])", Array{9223372036854775808ull});
    checkJson(R"([""])",    Array{""});
    checkJson(R"([[]])",    Array{Array{}});
    checkJson(R"([{}])",    Array{Object{}});
    checkJson(R"([null,false,true,42,-42,"hello","\u0000Qg==",[],{}])",
              Array{null,false,true,42u,-42,"hello",Blob{0x42},Array{},Object{}});
    checkJson(R"([[["foo",42]],[{"foo":42}]])",
              Array{ Array{Array{"foo",42u} }, Array{ Object{{"foo",42u}} } });
    checkJson(R"({})",          Object{});
    checkJson(R"({"":""})",     Object{ {"",""} });
    checkJson(R"({"n":null})",  Object{ {"n",null} });
    checkJson(R"({"b":false})", Object{ {"b",false} });
    checkJson(R"({"b":true})",  Object{ {"b",true} });
    checkJson(R"({"n":0})",     Object{ {"n",0u} });
    checkJson(R"({"n":-1})",    Object{ {"n",-1} });
    checkJson(R"({"n":9223372036854775807})", Object{ {"n",(UInt)intMax} });
    checkJson(R"({"n":9223372036854775808})", Object{ {"n",9223372036854775808ull} });
    checkJson(R"({"s":""})",    Object{ {"s","" } });
    checkJson(R"({"a":[]})",    Object{ {"a",Array{}} });
    checkJson(R"({"o":{}})",    Object{ {"o",Object{}} });
    checkJson(R"({"":null,"f":false,"t":true,"u":0,"n":-1,"s":"abc","b":"\u0000Qg==","a":[],"o":{}})",
              Object{ {"",null}, {"b",Blob{0x42}}, {"f",false}, {"t",true}, {"u",0u},
                      {"n",-1}, {"s","abc"}, {"a",Array{}}, {"o",Object{}} },
                R"({"":null,"a":[],"b":"\u0000Qg==","f":false,"n":-1,"o":{},"s":"abc","t":true,"u":0})");
    checkJson(R"({"a":{"b":{"c":42}}})",
              Object{ {"a", Object{ {"b", Object{ {"c",42u} }} } } });
}
GIVEN( "invalid JSON strings" )
{
    checkError("");
    checkError("nil");
    checkError("t");
    checkError("f");
    checkError(R"(!%#($)%*$)");
    checkError(R"(42!)");
    checkError(R"(Hello)");
    checkError(R"(Hello)");
    checkError(R"("\u0000====")");
    checkError(R"("\u0000A===")");
    checkError(R"("\u0000AA=A")");
    checkError(R"("\u0000=AA=")");
    checkError(R"("\u0000A")");
    checkError(R"("\u0000AA==A")");
    checkError(R"("\u0000AAAAA")");
    checkError(R"("\u0000AA=")");
    checkError(R"("\u0000AAA ")");
    checkError(R"("\u0000AAA.")");
    checkError(R"("\u0000AAA:")");
    checkError(R"("\u0000AAA@")");
    checkError(R"("\u0000AAA[")");
    checkError(R"("\u0000AAA`")");
    checkError(R"("\u0000AAA{")");
    checkError(R"("\u0000AAA-")");
    checkError(R"("\u0000AAA_")");
    checkError(R"([42,false,"Hello)");
    checkError(R"([42,false,"Hello]])");
    checkError(R"([42,false,"Hello})");
    checkError(R"([42,false,[])");
    checkError(R"({"foo"})");
    checkError(R"({"foo","bar"})");
    checkError(R"({"foo":"bar")");
    checkError(R"({"foo":"bar"])");
    checkError(R"({42:"bar"})");
}
GIVEN( "non-finite real numbers" )
{
    WHEN( "serializing NaN" )
    {
        Variant v(std::numeric_limits<Real>::quiet_NaN());
        std::string str;
        Json::encode(v, str);
        CHECK( std::isnan(v.as<Real>()) );
        CHECK_THAT( str, Equals("null") );
    }
    WHEN( "serializing positive infinity" )
    {
        Variant v(std::numeric_limits<Real>::infinity());
        std::string str;
        Json::encode(v, str);
        CHECK( std::isinf(v.as<Real>()) );
        CHECK_THAT( str, Equals("null") );
    }
    WHEN( "serializing negative infinity" )
    {
        if (std::numeric_limits<Real>::is_iec559)
        {
            Variant v(-std::numeric_limits<Real>::infinity());
            std::string str;
            Json::encode(v, str);
            CHECK( std::isinf(v.as<Real>()) );
            CHECK_THAT( str, Equals("null") );
        }
    }
}
GIVEN( "a string Variant with control characters" )
{
    std::string s;
    for (char c = 1; c<=0x1f; ++c)
        s += c;
    s += '\"';
    s += '\\';
    Variant v = s;

    WHEN( "encoding to JSON and decoding back" )
    {
        std::string encoded;
        Json::encode(v, encoded);
        Variant decoded;
        Json::decode(encoded, decoded);

        THEN( "the decoded Variant matches the original" )
        {
            CHECK( decoded == v );
        }
    }
}
GIVEN( "an object Variant with control characters in a key" )
{
    std::string key;
    for (char c = 1; c<=0x1f; ++c)
        key += c;
    key += '\"';
    key += '\\';
    Variant v = Object{{{key, 123}}};

    WHEN( "encoding to JSON and decoding back" )
    {
        std::string encoded;
        Json::encode(v, encoded);
        Variant decoded;
        Json::decode(encoded, decoded);

        THEN( "the decoded Variant matches the original" )
        {
            CHECK( decoded == v );
        }
    }
}
}

#endif // #if CPPWAMP_TESTING_CODEC
