/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include <sstream>
#include <vector>
#include <catch2/catch.hpp>
#include <cppwamp/variant.hpp>
#include <cppwamp/codecs/cbor.hpp>
#include <jsoncons_ext/cbor/cbor_options.hpp>

using namespace wamp;

namespace
{

//------------------------------------------------------------------------------
template <typename T, typename U>
void checkCbor(CborBufferEncoder& encoder, CborBufferDecoder& decoder,
               const T& value, const U& expected)
{
    INFO( "For value \"" << value << "\"" );
    Variant v(value);

    {
        MessageBuffer buffer;
        encoder.encode(v, buffer);
        Variant w;
        auto ec = decoder.decode(buffer, w);
        CHECK( !ec );
        CHECK( w == Variant(expected) );
    }

    {
        std::ostringstream oss;
        encode<Cbor>(v, oss);
        Variant w;
        std::istringstream iss(oss.str());
        auto ec = decode<Cbor>(iss, w);
        CHECK( !ec );
        CHECK( w == Variant(expected) );
    }
}

//------------------------------------------------------------------------------
template <typename T>
void checkCbor(CborBufferEncoder& encoder, CborBufferDecoder& decoder,
               const T& value)
{
    checkCbor(encoder, decoder, value, value);
}

} // anonymous namespace

//------------------------------------------------------------------------------
SCENARIO( "CBOR serialization", "[Variant][Codec][Cbor]" )
{
GIVEN( "an assortment of variants" )
{
    auto intMin  = std::numeric_limits<Int>::min();
    auto intMax  = std::numeric_limits<Int>::max();
    auto uintMax = std::numeric_limits<UInt>::max();
    auto floatMin = std::numeric_limits<float>::lowest();
    auto floatMax = std::numeric_limits<float>::max();
    auto realMin = std::numeric_limits<Real>::lowest();
    auto realMax = std::numeric_limits<Real>::max();

    CborBufferEncoder e;
    CborBufferDecoder d;

    checkCbor(e, d, null);
    checkCbor(e, d, false);
    checkCbor(e, d, true);
    checkCbor(e, d, 0u);
    checkCbor(e, d, 0, 0u);
    checkCbor(e, d, 1u);
    checkCbor(e, d, 1, 1u);
    checkCbor(e, d, -1);
    checkCbor(e, d, 127, 127u);
    checkCbor(e, d, 127u);
    checkCbor(e, d, -128);
    checkCbor(e, d, 255, 255u);
    checkCbor(e, d, 255u);
    checkCbor(e, d, -255);
    checkCbor(e, d, 32767, 32767u);
    checkCbor(e, d, 32767u);
    checkCbor(e, d, -32768);
    checkCbor(e, d, 65535, 65535u);
    checkCbor(e, d, 65535u);
    checkCbor(e, d, -65535);
    checkCbor(e, d, 2147483647, 2147483647u);
    checkCbor(e, d, 2147483647u);
    checkCbor(e, d, -2147483648);
    checkCbor(e, d, 4294967295, 4294967295u);
    checkCbor(e, d, 4294967295u);
    checkCbor(e, d, -4294967295);
    checkCbor(e, d, intMin);
    checkCbor(e, d, intMax, (UInt)intMax);
    checkCbor(e, d, (UInt)intMax);
    checkCbor(e, d, uintMax);
    checkCbor(e, d, 0.0f);
    checkCbor(e, d, 0.0);
    checkCbor(e, d, 42.1f);
    checkCbor(e, d, 42.1);
    checkCbor(e, d, -42.1f);
    checkCbor(e, d, -42.1);
    checkCbor(e, d, floatMin);
    checkCbor(e, d, floatMax);
    checkCbor(e, d, realMin);
    checkCbor(e, d, realMax);
    checkCbor(e, d, "");
    checkCbor(e, d, "Hello");
    checkCbor(e, d, "null");
    checkCbor(e, d, "false");
    checkCbor(e, d, "true");
    checkCbor(e, d, "0");
    checkCbor(e, d, "1");
    checkCbor(e, d, Blob{});
    checkCbor(e, d, Blob{0x00});
    checkCbor(e, d, Blob{0x01, 0x02, 0x03});
    checkCbor(e, d, Array{});
    checkCbor(e, d, Array{null});
    checkCbor(e, d, Array{false});
    checkCbor(e, d, Array{true});
    checkCbor(e, d, Array{42u});
    checkCbor(e, d, Array{42}, Array{42u});
    checkCbor(e, d, Array{-42});
    checkCbor(e, d, Array{intMax}, Array{(UInt)intMax});
    checkCbor(e, d, Array{(UInt)intMax});
    checkCbor(e, d, Array{42.1});
    checkCbor(e, d, Array{-42.1});
    checkCbor(e, d, Array{floatMin});
    checkCbor(e, d, Array{floatMax});
    checkCbor(e, d, Array{realMin});
    checkCbor(e, d, Array{realMax});
    checkCbor(e, d, Array{""});
    checkCbor(e, d, Array{Array{}});
    checkCbor(e, d, Array{Object{}});
    checkCbor(e, d, Array{null,false,true,42u,-42,42.1,"hello",Array{},Object{}});
    checkCbor(e, d, Array{ Array{Array{"foo",42u} }, Array{ Object{{"foo",42.1}} } });
    checkCbor(e, d, Object{});
    checkCbor(e, d, Object{ {"",""} });
    checkCbor(e, d, Object{ {"n",null} });
    checkCbor(e, d, Object{ {"b",false} });
    checkCbor(e, d, Object{ {"b",true} });
    checkCbor(e, d, Object{ {"n",0u} });
    checkCbor(e, d, Object{ {"n",-1} });
    checkCbor(e, d, Object{ {"n",intMax} }, Object{ {"n",(UInt)intMax} });
    checkCbor(e, d, Object{ {"n",(UInt)intMax} });
    checkCbor(e, d, Object{ {"x",42.1} });
    checkCbor(e, d, Object{ {"x",-42.1} });
    checkCbor(e, d, Object{ {"x",floatMin} });
    checkCbor(e, d, Object{ {"x",floatMax} });
    checkCbor(e, d, Object{ {"x",realMin} });
    checkCbor(e, d, Object{ {"x",realMax} });
    checkCbor(e, d, Object{ {"s","" } });
    checkCbor(e, d, Object{ {"a",Array{}} });
    checkCbor(e, d, Object{ {"o",Object{}} });
    checkCbor(e, d, Object{ {"",null}, {"f",false}, {"t",true}, {"u",0u}, {"n",-1},
                            {"x",42.1}, {"s","abc"}, {"a",Array{}}, {"o",Object{}} });
    checkCbor(e, d, Object{ {"a", Object{ {"b", Object{ {"c",42u} }} } } });
}
GIVEN( "an empty CBOR message" )
{
    MessageBuffer empty;
    Variant v;
    CborBufferDecoder decoder;
    auto ec = decoder.decode(empty, v);
    CHECK_FALSE( !ec );
    CHECK( ec == DecodingErrc::failed );
    CHECK( ec == jsoncons::cbor::cbor_errc::unexpected_eof );

    WHEN( "decoding a valid message after an error" )
    {
        MessageBuffer buffer = {0x18, 0x2a};
        ec = decoder.decode(buffer, v);
        CHECK( !ec );
        CHECK(v == 42);
    }
}
GIVEN( "an invalid CBOR message" )
{
    std::ostringstream oss;
    oss << uint8_t(0xe0);
    Variant v;
    auto ec = decode<Cbor>(oss.str(), v);
    CHECK_FALSE( !ec );
    CHECK( ec == DecodingErrc::failed );
    CHECK( ec == jsoncons::cbor::cbor_errc::unknown_type );
}
GIVEN( "a short CBOR message" )
{
    MessageBuffer buffer{0x65, 'h', 'e', 'l', 'l'}; // 5-byte text string
    Variant v;
    CborBufferDecoder decoder;
    auto ec = decoder.decode(buffer, v);
    CHECK_FALSE( !ec );
    CHECK( ec == DecodingErrc::failed );
    CHECK( ec == jsoncons::cbor::cbor_errc::unexpected_eof );

    WHEN( "decoding a valid message after an error" )
    {
        MessageBuffer buffer = {0x18, 0x2a};
        auto ec = decoder.decode(buffer, v);
        CHECK( !ec );
        CHECK(v == 42);
    }
}
GIVEN( "a CBOR message with a non-string key" )
{
    MessageBuffer buffer{0xA1, 0x01, 0x2}; // {1:2}
    Variant v;
    CborBufferDecoder decoder;
    auto ec = decoder.decode(buffer, v);
    CHECK( ec == DecodingErrc::failed );
    CHECK( ec == DecodingErrc::expectedStringKey );

    WHEN( "decoding a valid message after an error" )
    {
        MessageBuffer buffer = {0x18, 0x2a};
        auto ec = decoder.decode(buffer, v);
        CHECK( !ec );
        CHECK(v == 42);
    }
}
}

//------------------------------------------------------------------------------
SCENARIO( "CBOR typed array", "[Variant][Codec][Cbor]" )
{
    std::vector<uint8_t> input{
        0xD8, 0x41, // tag(65) uint16 big endian typed Array
        0x48,       // bytes (8)
        0x01, 0x23, // uint16_t(291)
        0x45, 0x67, // uint16_t(17767)
        0x89, 0xAB, // uint16_t(35243)
        0xCD, 0xEF  // uint16_t(52719)
    };

    Variant v;
    auto ec = decode<Cbor>(input, v);
    CHECK( !ec );
    REQUIRE( v.is<Array>() );
    const auto& a = v.as<Array>();
    REQUIRE( a.size() == 4 );
    CHECK( a[0] == 291 );
    CHECK( a[1] == 17767 );
    CHECK( a[2] == 35243 );
    CHECK( a[3] == 52719 );
}

//------------------------------------------------------------------------------
SCENARIO( "CBOR options", "[Variant][Codec][Cbor]" )
{
    jsoncons::cbor::cbor_options cborOptions;
    cborOptions.max_nesting_depth(2);
    cborOptions.pack_strings(true);

    CborOptions options(cborOptions);
    AnyBufferCodec codec{options};

    WHEN( "encoding with options" )
    {
        Variant v{Array{"foo", "foo"}};
        MessageBuffer output;
        MessageBuffer expected{
            0x82,                  // array(2)
                0x63,              // text(3)
                    'f', 'o', 'o', // "foo"
                0xD8, 0x19,        // tag(25), reference previous string
                00                 // unsigned(0)
        };

        codec.encode(v, output);
        CHECK_THAT(output, Catch::Matchers::Equals(expected));

        output.clear();
        wamp::encode(v, options, output);
        CHECK_THAT(output, Catch::Matchers::Equals(expected));
    }

    WHEN( "decoding with options" )
    {
        MessageBuffer input{
            0x81,                  // array(1)
                0x81,              // array(1)
                    0x81,          // array(1)
                        0x18, 0x2A // unsigned(42)
        };

        Variant v;
        auto ec = codec.decode(input, v);
        CHECK(ec == jsoncons::cbor::cbor_errc::max_nesting_depth_exceeded);

        ec = wamp::decode(input, options, v);
        CHECK(ec == jsoncons::cbor::cbor_errc::max_nesting_depth_exceeded);
    }
}
