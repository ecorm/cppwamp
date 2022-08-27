/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include <sstream>
#include <catch2/catch.hpp>
#include <cppwamp/variant.hpp>
#include <cppwamp/msgpack.hpp>

using namespace wamp;

namespace
{

//------------------------------------------------------------------------------
template <typename T, typename U>
void checkMsgpack(MsgpackBufferEncoder& encoder, MsgpackBufferDecoder& decoder,
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
        encode<Msgpack>(v, oss);
        Variant w;
        std::istringstream iss(oss.str());
        auto ec = decode<Msgpack>(iss, w);
        CHECK( !ec );
        CHECK( w == Variant(expected) );
    }
}

//------------------------------------------------------------------------------
template <typename T>
void checkMsgpack(MsgpackBufferEncoder& encoder, MsgpackBufferDecoder& decoder,
                  const T& value)
{
    checkMsgpack(encoder, decoder, value, value);
}

} // anonymous namespace

//------------------------------------------------------------------------------
SCENARIO( "Msgpack serialization", "[Variant][Codec][Msgpack]" )
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

    MsgpackBufferEncoder e;
    MsgpackBufferDecoder d;

    checkMsgpack(e, d, null);
    checkMsgpack(e, d, false);
    checkMsgpack(e, d, true);
    checkMsgpack(e, d, 0u);
    checkMsgpack(e, d, 0, 0u);
    checkMsgpack(e, d, 1u);
    checkMsgpack(e, d, 1, 1u);
    checkMsgpack(e, d, -1);
    checkMsgpack(e, d, 127, 127u);
    checkMsgpack(e, d, 127u);
    checkMsgpack(e, d, -128);
    checkMsgpack(e, d, 255, 255u);
    checkMsgpack(e, d, 255u);
    checkMsgpack(e, d, -255);
    checkMsgpack(e, d, 32767, 32767u);
    checkMsgpack(e, d, 32767u);
    checkMsgpack(e, d, -32768);
    checkMsgpack(e, d, 65535, 65535u);
    checkMsgpack(e, d, 65535u);
    checkMsgpack(e, d, -65535);
    checkMsgpack(e, d, 2147483647, 2147483647u);
    checkMsgpack(e, d, 2147483647u);
    checkMsgpack(e, d, -2147483648);
    checkMsgpack(e, d, 4294967295, 4294967295u);
    checkMsgpack(e, d, 4294967295u);
    checkMsgpack(e, d, -4294967295);
    checkMsgpack(e, d, intMin);
    checkMsgpack(e, d, intMax, (UInt)intMax);
    checkMsgpack(e, d, (UInt)intMax);
    checkMsgpack(e, d, uintMax);
    checkMsgpack(e, d, 0.0f);
    checkMsgpack(e, d, 0.0);
    checkMsgpack(e, d, 42.1f);
    checkMsgpack(e, d, 42.1);
    checkMsgpack(e, d, -42.1f);
    checkMsgpack(e, d, -42.1);
    checkMsgpack(e, d, floatMin);
    checkMsgpack(e, d, floatMax);
    checkMsgpack(e, d, realMin);
    checkMsgpack(e, d, realMax);
    checkMsgpack(e, d, "");
    checkMsgpack(e, d, "Hello");
    checkMsgpack(e, d, "null");
    checkMsgpack(e, d, "false");
    checkMsgpack(e, d, "true");
    checkMsgpack(e, d, "0");
    checkMsgpack(e, d, "1");
    checkMsgpack(e, d, Blob{});
    checkMsgpack(e, d, Blob{0x00});
    checkMsgpack(e, d, Blob{0x01, 0x02, 0x03});
    checkMsgpack(e, d, Array{});
    checkMsgpack(e, d, Array{null});
    checkMsgpack(e, d, Array{false});
    checkMsgpack(e, d, Array{true});
    checkMsgpack(e, d, Array{42u});
    checkMsgpack(e, d, Array{42}, Array{42u});
    checkMsgpack(e, d, Array{-42});
    checkMsgpack(e, d, Array{intMax}, Array{(UInt)intMax});
    checkMsgpack(e, d, Array{(UInt)intMax});
    checkMsgpack(e, d, Array{42.1});
    checkMsgpack(e, d, Array{-42.1});
    checkMsgpack(e, d, Array{floatMin});
    checkMsgpack(e, d, Array{floatMax});
    checkMsgpack(e, d, Array{realMin});
    checkMsgpack(e, d, Array{realMax});
    checkMsgpack(e, d, Array{""});
    checkMsgpack(e, d, Array{Array{}});
    checkMsgpack(e, d, Array{Object{}});
    checkMsgpack(e, d, Array{null,false,true,42u,-42,42.1,"hello",Array{},Object{}});
    checkMsgpack(e, d, Array{ Array{Array{"foo",42u} }, Array{ Object{{"foo",42.1}} } });
    checkMsgpack(e, d, Object{});
    checkMsgpack(e, d, Object{ {"",""} });
    checkMsgpack(e, d, Object{ {"n",null} });
    checkMsgpack(e, d, Object{ {"b",false} });
    checkMsgpack(e, d, Object{ {"b",true} });
    checkMsgpack(e, d, Object{ {"n",0u} });
    checkMsgpack(e, d, Object{ {"n",-1} });
    checkMsgpack(e, d, Object{ {"n",intMax} }, Object{ {"n",(UInt)intMax} });
    checkMsgpack(e, d, Object{ {"n",(UInt)intMax} });
    checkMsgpack(e, d, Object{ {"x",42.1} });
    checkMsgpack(e, d, Object{ {"x",-42.1} });
    checkMsgpack(e, d, Object{ {"x",floatMin} });
    checkMsgpack(e, d, Object{ {"x",floatMax} });
    checkMsgpack(e, d, Object{ {"x",realMin} });
    checkMsgpack(e, d, Object{ {"x",realMax} });
    checkMsgpack(e, d, Object{ {"s","" } });
    checkMsgpack(e, d, Object{ {"a",Array{}} });
    checkMsgpack(e, d, Object{ {"o",Object{}} });
    checkMsgpack(e, d, Object{ {"",null}, {"f",false}, {"t",true}, {"u",0u}, {"n",-1},
                               {"x",42.1}, {"s","abc"}, {"a",Array{}}, {"o",Object{}} });
    checkMsgpack(e, d, Object{ {"a", Object{ {"b", Object{ {"c",42u} }} } } });
}
GIVEN( "an empty Msgpack message" )
{
    MessageBuffer empty;
    Variant v;
    MsgpackBufferDecoder decoder;
    auto ec = decoder.decode(empty, v);
    CHECK_FALSE( !ec );
    CHECK( ec == DecodingErrc::failure );
    CHECK( ec == jsoncons::msgpack::msgpack_errc::unexpected_eof );

    WHEN( "decoding a valid message after an error" )
    {
        MessageBuffer buffer = {0x2a};
        ec = decoder.decode(buffer, v);
        CHECK( !ec );
        CHECK(v == 42);
    }
}
GIVEN( "an invalid Msgpack message" )
{
    std::ostringstream oss;
    oss << uint8_t(0xc1);
    Variant v;
    auto ec = decode<Msgpack>(oss.str(), v);
    CHECK_FALSE( !ec );
    CHECK( ec == DecodingErrc::failure );
    CHECK( ec == jsoncons::msgpack::msgpack_errc::unknown_type );
}
GIVEN( "a short Msgpack message" )
{
    MessageBuffer buffer{0xa5, 'h', 'e', 'l', 'l'}; // 5-byte text string
    Variant v;
    MsgpackBufferDecoder decoder;
    auto ec = decoder.decode(buffer, v);
    CHECK_FALSE( !ec );
    CHECK( ec == DecodingErrc::failure );
    CHECK( ec == jsoncons::msgpack::msgpack_errc::unexpected_eof );

    WHEN( "decoding a valid message after an error" )
    {
        MessageBuffer buffer = {0x2a};
        auto ec = decoder.decode(buffer, v);
        CHECK( !ec );
        CHECK(v == 42);
    }
}
GIVEN( "a Msgpack message with a non-string key" )
{
    MessageBuffer buffer{0x82, 0x01, 0x2}; // {1:2}
    Variant v;
    MsgpackBufferDecoder decoder;
    auto ec = decoder.decode(buffer, v);
    CHECK( ec == DecodingErrc::failure );
    CHECK( ec == DecodingErrc::expectedStringKey );

    WHEN( "decoding a valid message after an error" )
    {
        MessageBuffer buffer = {0x2a};
        auto ec = decoder.decode(buffer, v);
        CHECK( !ec );
        CHECK(v == 42);
    }
}

}
