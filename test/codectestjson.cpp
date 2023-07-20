/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include <cmath>
#include <limits>
#include <sstream>
#include <catch2/catch.hpp>
#include <cppwamp/variant.hpp>
#include <cppwamp/json.hpp>
#include <jsoncons/json_options.hpp>

using namespace wamp;
namespace Matchers = Catch::Matchers;

namespace
{

//------------------------------------------------------------------------------
template <typename T>
void checkJson(JsonStringEncoder& encoder, JsonStringDecoder& decoder,
               const std::string& json, const T& value,
               const std::string& serialized)
{
    INFO( "For JSON string \"" << json << "\"" );
    Variant expected(value);

    {
        Variant v;
        auto ec = decoder.decode(json, v);
        CHECK( !ec );
        CHECK( v == expected );

        std::string str;
        encoder.encode(v, str);
        CHECK( v == expected );
        CHECK( str == serialized );

        std::ostringstream oss;
        encode<Json>(v, oss);
        CHECK( v == expected );
        CHECK( oss.str() == serialized );

        std::string stringified = toString(v);
        CHECK( stringified == serialized );
    }

    {
        Variant v;
        std::istringstream iss(json);
        CHECK_NOTHROW( decode<Json>(iss, v) );
        CHECK( v == expected );
    }
}

//------------------------------------------------------------------------------
template <typename T>
void checkJson(JsonStringEncoder& encoder, JsonStringDecoder& decoder,
               const std::string& json, const T& value)
{
    checkJson(encoder, decoder, json, value, json);
}

//------------------------------------------------------------------------------
template <typename TExpected, typename TInteger>
void checkInteger(const std::string& json, TInteger n)
{
    INFO( "For JSON string \"" << json << "\"" );

    {
        Variant v;
        CHECK_NOTHROW( decode<Json>(json, v) );
        REQUIRE( v.is<TExpected>() );
        CHECK( v.as<TExpected>() == n );
        CHECK( v == n );
    }

    {
        Variant v;
        std::istringstream iss(json);
        CHECK_NOTHROW( decode<Json>(iss, v) );
        REQUIRE( v.is<TExpected>() );
        CHECK( v.as<TExpected>() == n );
        CHECK( v == n );
    }
}

//------------------------------------------------------------------------------
void checkReal(const std::string& json, double x)
{
    INFO( "For JSON string \"" << json << "\"" );
    auto margin = std::numeric_limits<Real>::epsilon()*10.0;
    {
        Variant v;
        CHECK_NOTHROW( decode<Json>(json, v) );
        REQUIRE( v.is<Real>() );
        CHECK( v.as<Real>() == Approx(x).margin(margin) );
    }

    {
        Variant v;
        std::istringstream iss(json);
        CHECK_NOTHROW( decode<Json>(iss, v) );
        REQUIRE( v.is<Real>() );
        CHECK( v.as<Real>() == Approx(x).margin(margin) );
    }
}

//------------------------------------------------------------------------------
template <typename TErrc>
void checkError(JsonStringDecoder& decoder, const std::string& json, TErrc errc)
{
    INFO( "For JSON string \"" << json << "\"" );

    {
        auto originalValue = Array{null, true, 42, "hello"};
        Variant v(originalValue);
        auto ec = decoder.decode(json, v);
        CHECK_FALSE( !ec );
        CHECK( ec == DecodingErrc::failed );
        CHECK( ec == errc );
        CHECK( v == originalValue );
    }
}

} // anonymous namespace

//------------------------------------------------------------------------------
SCENARIO( "JSON serialization", "[Variant][Codec][JSON][thisone]" )
{
GIVEN( "valid JSON numeric strings" )
{
    Int intMin  = std::numeric_limits<Int>::min();
    Int intMax  = std::numeric_limits<Int>::max();
    UInt uintMax = std::numeric_limits<UInt>::max();

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
    Int intMax  = std::numeric_limits<Int>::max();
    JsonStringEncoder e;
    JsonStringDecoder d;

    checkJson(e, d, R"(null)",    null);
    checkJson(e, d, R"(false)",   false);
    checkJson(e, d, R"(true)",    true);
    checkJson(e, d, R"("")", "");
    checkJson(e, d, R"("Hello")", "Hello");
    checkJson(e, d, R"("null")",  "null");
    checkJson(e, d, R"("false")", "false");
    checkJson(e, d, R"("true")",  "true");
    checkJson(e, d, R"("0")",     "0");
    checkJson(e, d, R"("1")",     "1");
    checkJson(e, d, R"("\u0000")",         Blob{});
    checkJson(e, d, R"("\u0000AA==")",     Blob{0x00});
    checkJson(e, d, R"("\u0000Zg==")",     Blob{'f'});
    checkJson(e, d, R"("\u0000Zm8=")",     Blob{'f','o'});
    checkJson(e, d, R"("\u0000Zm9v")",     Blob{'f','o','o'});
    checkJson(e, d, R"("\u0000Zm9vYg==")", Blob{'f','o','o','b'});
    checkJson(e, d, R"("\u0000Zm9vYmE=")", Blob{'f','o','o','b','a'});
    checkJson(e, d, R"("\u0000Zm9vYmFy")", Blob{'f','o','o','b','a','r'});
    checkJson(e, d, R"("\u0000FPucAw==")", Blob{0x14, 0xfb, 0x9c, 0x03});
    checkJson(e, d, R"("\u0000FPucA9k=")", Blob{0x14, 0xfb, 0x9c, 0x03, 0xd9});
    checkJson(e, d, R"("\u0000FPucA9l+")", Blob{0x14, 0xfb, 0x9c, 0x03, 0xd9, 0x7e});
    checkJson(e, d, R"([])",      Array{});
    checkJson(e, d, R"([null])",  Array{null});
    checkJson(e, d, R"([false])", Array{false});
    checkJson(e, d, R"([true])",  Array{true});
    checkJson(e, d, R"([0])",     Array{0u});
    checkJson(e, d, R"([-1])",    Array{-1});
    checkJson(e, d, R"([9223372036854775807])", Array{(UInt)intMax});
    checkJson(e, d, R"([9223372036854775808])", Array{9223372036854775808ull});
    checkJson(e, d, R"([""])",    Array{""});

    // Array{Array{}} is ambiguous with the move constructor and the
    // constructor taking an initializer list
    checkJson(e, d, R"([[]])",    Array{Variant{Array{}}});
    checkJson(e, d, R"([{}])",    Array{Object{}});
    checkJson(e, d, R"([null,false,true,42,-42,"hello","\u0000Qg==",[],{}])",
              Array{null,false,true,42u,-42,"hello",Blob{0x42},Array{},Object{}});
    checkJson(e, d, R"([[["foo",42]],[{"foo":42}]])",
              Array{ Variant{Array{Variant{Array{"foo",42u}}}},
                     Array{Object{{"foo",42u}}} });
    checkJson(e, d, R"({})",          Object{});
    checkJson(e, d, R"({"":""})",     Object{ {"",""} });
    checkJson(e, d, R"({"n":null})",  Object{ {"n",null} });
    checkJson(e, d, R"({"b":false})", Object{ {"b",false} });
    checkJson(e, d, R"({"b":true})",  Object{ {"b",true} });
    checkJson(e, d, R"({"n":0})",     Object{ {"n",0u} });
    checkJson(e, d, R"({"n":-1})",    Object{ {"n",-1} });
    checkJson(e, d, R"({"n":9223372036854775807})", Object{ {"n",(UInt)intMax} });
    checkJson(e, d, R"({"n":9223372036854775808})", Object{ {"n",9223372036854775808ull} });
    checkJson(e, d, R"({"s":""})",    Object{ {"s","" } });
    checkJson(e, d, R"({"a":[]})",    Object{ {"a",Array{}} });
    checkJson(e, d, R"({"o":{}})",    Object{ {"o",Object{}} });
    checkJson(e, d, R"({"":null,"f":false,"t":true,"u":0,"n":-1,"s":"abc","b":"\u0000Qg==","a":[],"o":{}})",
              Object{ {"",null}, {"b",Blob{0x42}}, {"f",false}, {"t",true}, {"u",0u},
                      {"n",-1}, {"s","abc"}, {"a",Array{}}, {"o",Object{}} },
                R"({"":null,"a":[],"b":"\u0000Qg==","f":false,"n":-1,"o":{},"s":"abc","t":true,"u":0})");
    checkJson(e, d, R"({"a":{"b":{"c":42}}})",
              Object{ {"a", Object{ {"b", Object{ {"c",42u} }} } } });
}
GIVEN( "invalid JSON strings" )
{
    using DE = DecodingErrc;
    using JE = jsoncons::json_errc;

    JsonStringDecoder d;

    checkError(d, "",                      DE::emptyInput );
    checkError(d, " ",                     DE::emptyInput);
    checkError(d, "// comment",            JE::illegal_comment);
    checkError(d, "/* comment */",         JE::illegal_comment);
    checkError(d, "[null // comment]",     JE::illegal_comment);
    checkError(d, "[null /* comment */]",  JE::illegal_comment);
    checkError(d, "nil",                   JE::invalid_value );
    checkError(d, "t",                     JE::unexpected_eof);
    checkError(d, "f",                     JE::unexpected_eof);
    checkError(d, R"(!%#($)%*$)",          JE::syntax_error);
    checkError(d, R"(42!)",                JE::invalid_number);
    checkError(d, R"(Hello)",              JE::syntax_error);
    checkError(d, R"(Hello)",              JE::syntax_error);
    checkError(d, R"("\u0000====")",       DE::badBase64Padding );
    checkError(d, R"("\u0000A===")",       DE::badBase64Padding);
    checkError(d, R"("\u0000AA=A")",       DE::badBase64Padding);
    checkError(d, R"("\u0000=AA=")",       DE::badBase64Padding);
    checkError(d, R"("\u0000A")",          DE::badBase64Length);
    checkError(d, R"("\u0000AA==A")",      DE::badBase64Padding);
    checkError(d, R"("\u0000AAAAA")",      DE::badBase64Length);
    checkError(d, R"("\u0000AAA ")",       DE::badBase64Char);
    checkError(d, R"("\u0000AAA.")",       DE::badBase64Char);
    checkError(d, R"("\u0000AAA:")",       DE::badBase64Char);
    checkError(d, R"("\u0000AAA@")",       DE::badBase64Char);
    checkError(d, R"("\u0000AAA[")",       DE::badBase64Char);
    checkError(d, R"("\u0000AAA`")",       DE::badBase64Char);
    checkError(d, R"("\u0000AAA{")",       DE::badBase64Char);
    checkError(d, R"("\u0000AAA-")",       DE::badBase64Char);
    checkError(d, R"("\u0000AAA_")",       DE::badBase64Char);
    checkError(d, R"([42,false,"Hello)",   JE::unexpected_eof);
    checkError(d, R"([42,false,"Hello]])", JE::unexpected_eof);
    checkError(d, R"([42,false,"Hello})",  JE::unexpected_eof);
    checkError(d, R"([42,false,[])",       JE::unexpected_eof);
    checkError(d, R"({"foo"})",            JE::expected_colon);
    checkError(d, R"({"foo","bar"})",      JE::expected_colon);
    checkError(d, R"({"foo":"bar")",       JE::unexpected_eof);
    checkError(d, R"({"foo":"bar"])",      JE::expected_comma_or_rbrace);
    checkError(d, R"({42:"bar"})",         JE::expected_key);

    WHEN( "decoding a valid JSON string after an error" )
    {
        std::string json("42");
        Variant v;
        auto ec = d.decode(json, v);
        CHECK( !ec );
        CHECK( v == 42 );
    }
}
GIVEN( "non-finite real numbers" )
{
    WHEN( "serializing NaN" )
    {
        Variant v(std::numeric_limits<Real>::quiet_NaN());
        std::string str;
        encode<Json>(v, str);
        CHECK( std::isnan(v.as<Real>()) );
        CHECK_THAT( str, Matchers::Equals("null") );
    }
    WHEN( "serializing positive infinity" )
    {
        Variant v(std::numeric_limits<Real>::infinity());
        std::string str;
        encode<Json>(v, str);
        CHECK( std::isinf(v.as<Real>()) );
        CHECK_THAT( str, Matchers::Equals("null") );
    }
    WHEN( "serializing negative infinity" )
    {
        if (std::numeric_limits<Real>::is_iec559)
        {
            Variant v(-std::numeric_limits<Real>::infinity());
            std::string str;
            encode<Json>(v, str);
            CHECK( std::isinf(v.as<Real>()) );
            CHECK_THAT( str, Matchers::Equals("null") );
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
        encode<Json>(v, encoded);
        Variant decoded;
        auto ec = decode<Json>(encoded, decoded);
        CHECK( !ec );

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
        encode<Json>(v, encoded);
        Variant decoded;
        auto ec = decode<Json>(encoded, decoded);
        CHECK( !ec );

        THEN( "the decoded Variant matches the original" )
        {
            CHECK( decoded == v );
        }
    }
}
GIVEN( "a string Variant with multi-byte UTF-8 characters" )
{
    std::string s = "\u0080\u07ff\u0800\uffff\u00010000\u0010ffff";
    Variant v = s;

    WHEN( "encoding to JSON and decoding back" )
    {
        std::string encoded;
        encode<Json>(v, encoded);
        Variant decoded;
        auto ec = decode<Json>(encoded, decoded);
        CHECK( !ec );

        THEN( "the decoded Variant matches the original" )
        {
            CHECK( decoded == v );
        }
    }
}
}

//------------------------------------------------------------------------------
SCENARIO( "Json options", "[Variant][Codec][Json]" )
{
    jsoncons::json_options jsonOptions;
    jsonOptions.max_nesting_depth(2);
    jsonOptions.float_format(jsoncons::float_chars_format::fixed);
    jsonOptions.precision(2);

    JsonOptions options(jsonOptions);
    AnyStringCodec codec{options};

    WHEN( "encoding with options" )
    {
        Variant v{1.1};
        std::string output;
        std::string expected{"1.10"};

        codec.encode(v, output);
        CHECK(output == expected);

        output.clear();
        wamp::encode(v, options, output);
        CHECK(output == expected);
    }

    WHEN( "decoding with options" )
    {
        std::string input{"[[[42]]]"};

        Variant v;
        auto ec = codec.decode(input, v);
        CHECK(ec == jsoncons::json_errc::max_nesting_depth_exceeded);

        ec = wamp::decode(input, options, v);
        CHECK(ec == jsoncons::json_errc::max_nesting_depth_exceeded);
    }
}
