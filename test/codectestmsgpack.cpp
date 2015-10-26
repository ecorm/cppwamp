/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#if CPPWAMP_TESTING_CODEC

#include <sstream>
#include <catch.hpp>
#include <cppwamp/variant.hpp>
#include <cppwamp/msgpack.hpp>

using namespace wamp;

namespace
{

//------------------------------------------------------------------------------
template <typename T, typename U>
void checkMsgpack(const T& value, const U& expected)
{
    INFO( "For value \"" << value << "\"" );
    Variant v(value);

    {
        std::string str;
        CHECK_NOTHROW( Msgpack::encode(v, str) );
        CHECK( v == Variant(value) );
        Variant w;
        CHECK_NOTHROW( Msgpack::decode(str, w) );
        CHECK( w == Variant(expected) );
    }

    {
        std::ostringstream oss;
        CHECK_NOTHROW( Msgpack::encode(v, oss) );
        CHECK( v == Variant(value) );
        Variant w;
        CHECK_NOTHROW( Msgpack::decode(oss.str(), w) );
        CHECK( w == Variant(expected) );
    }
}

//------------------------------------------------------------------------------
template <typename T>
void checkMsgpack(const T& value)
{
    checkMsgpack(value, value);
}

} // anonymous namespace

//------------------------------------------------------------------------------
SCENARIO( "Msgpack serialization", "[Variant]" )
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

    checkMsgpack(null);
    checkMsgpack(false);
    checkMsgpack(true);
    checkMsgpack(0u);
    checkMsgpack(0, 0u);
    checkMsgpack(1u);
    checkMsgpack(1, 1u);
    checkMsgpack(-1);
    checkMsgpack(127, 127u);
    checkMsgpack(127u);
    checkMsgpack(-128);
    checkMsgpack(255, 255u);
    checkMsgpack(255u);
    checkMsgpack(-255);
    checkMsgpack(32767, 32767u);
    checkMsgpack(32767u);
    checkMsgpack(-32768);
    checkMsgpack(65535, 65535u);
    checkMsgpack(65535u);
    checkMsgpack(-65535);
    checkMsgpack(2147483647, 2147483647u);
    checkMsgpack(2147483647u);
    checkMsgpack(-2147483648);
    checkMsgpack(4294967295, 4294967295u);
    checkMsgpack(4294967295u);
    checkMsgpack(-4294967295);
    checkMsgpack(intMin);
    checkMsgpack(intMax, (UInt)intMax);
    checkMsgpack((UInt)intMax);
    checkMsgpack(uintMax);
    checkMsgpack(0.0f);
    checkMsgpack(0.0);
    checkMsgpack(42.1f);
    checkMsgpack(42.1);
    checkMsgpack(-42.1f);
    checkMsgpack(-42.1);
    checkMsgpack(floatMin);
    checkMsgpack(floatMax);
    checkMsgpack(realMin);
    checkMsgpack(realMax);
    checkMsgpack("");
    checkMsgpack("Hello");
    checkMsgpack("null");
    checkMsgpack("false");
    checkMsgpack("true");
    checkMsgpack("0");
    checkMsgpack("1");
    checkMsgpack(Blob{});
    checkMsgpack(Blob{0x00});
    checkMsgpack(Blob{0x01, 0x02, 0x03});
    checkMsgpack(Array{});
    checkMsgpack(Array{null});
    checkMsgpack(Array{false});
    checkMsgpack(Array{true});
    checkMsgpack(Array{42u});
    checkMsgpack(Array{42}, Array{42u});
    checkMsgpack(Array{-42});
    checkMsgpack(Array{intMax}, Array{(UInt)intMax});
    checkMsgpack(Array{(UInt)intMax});
    checkMsgpack(Array{42.1});
    checkMsgpack(Array{-42.1});
    checkMsgpack(Array{floatMin});
    checkMsgpack(Array{floatMax});
    checkMsgpack(Array{realMin});
    checkMsgpack(Array{realMax});
    checkMsgpack(Array{""});
    checkMsgpack(Array{Array{}});
    checkMsgpack(Array{Object{}});
    checkMsgpack(Array{null,false,true,42u,-42,42.1,"hello",Array{},Object{}});
    checkMsgpack(Array{ Array{Array{"foo",42u} }, Array{ Object{{"foo",42.1}} } });
    checkMsgpack(Object{});
    checkMsgpack(Object{ {"",""} });
    checkMsgpack(Object{ {"n",null} });
    checkMsgpack(Object{ {"b",false} });
    checkMsgpack(Object{ {"b",true} });
    checkMsgpack(Object{ {"n",0u} });
    checkMsgpack(Object{ {"n",-1} });
    checkMsgpack(Object{ {"n",intMax} }, Object{ {"n",(UInt)intMax} });
    checkMsgpack(Object{ {"n",(UInt)intMax} });
    checkMsgpack(Object{ {"x",42.1} });
    checkMsgpack(Object{ {"x",-42.1} });
    checkMsgpack(Object{ {"x",floatMin} });
    checkMsgpack(Object{ {"x",floatMax} });
    checkMsgpack(Object{ {"x",realMin} });
    checkMsgpack(Object{ {"x",realMax} });
    checkMsgpack(Object{ {"s","" } });
    checkMsgpack(Object{ {"a",Array{}} });
    checkMsgpack(Object{ {"o",Object{}} });
    checkMsgpack(Object{ {"",null}, {"f",false}, {"t",true}, {"u",0u}, {"n",-1},
                         {"x",42.1}, {"s","abc"}, {"a",Array{}}, {"o",Object{}} });
    checkMsgpack(Object{ {"a", Object{ {"b", Object{ {"c",42u} }} } } });
}
GIVEN( "an invalid Msgpack message" )
{
    std::ostringstream oss;
    oss << uint8_t(0xc1);
    Variant v;
    CHECK_THROWS_AS( Msgpack::decode(oss.str(), v), error::Decode );
}
GIVEN( "a short Msgpack message" )
{
    std::ostringstream oss;
    oss << uint8_t(0xa5) << "hell";
    Variant v;
    CHECK_THROWS_AS( Msgpack::decode(oss.str(), v), error::Decode );
}

}

#endif // #if CPPWAMP_TESTING_CODEC
