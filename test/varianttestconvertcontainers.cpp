/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#if CPPWAMP_TESTING_VARIANT

#include <catch.hpp>
#include <cppwamp/types/unorderedmap.hpp>

using namespace wamp;

namespace
{


} // anonymous namespace


//------------------------------------------------------------------------------
SCENARIO( "Converting to/from std::unordered_map", "[Variant]" )
{
GIVEN( "a valid variant object type" )
{
    Variant v(Object{{"a", 1},{"b", 2}});
    WHEN( "converting to a std::unordered_map" )
    {
        auto map = v.to<std::unordered_map<String, int>>();
        THEN( "the map is as expected" )
        {
            CHECK( map.size() == 2 );
            CHECK( map["a"] == 1 );
            CHECK( map["b"] == 2 );
        }
    }
}
GIVEN( "an empty variant object type" )
{
    Variant v(Object{});
    WHEN( "converting to a std::unordered_map" )
    {
        auto map = v.to<std::unordered_map<String, int>>();
        THEN( "the map is as expected" )
        {
            CHECK( map.empty() );
        }
    }
}
GIVEN( "a valid std::unordered_map type" )
{
    std::unordered_map<String, int> map{{"a", 1},{"b", 2}};
    WHEN( "converting to a variant" )
    {
        auto v = Variant::from(map);
        THEN( "the variant is as expected" )
        {
            CHECK( v == (Object{{"a", 1},{"b", 2}}) );
        }
    }
}
GIVEN( "an empty std::unordered_map" )
{
    std::unordered_map<String, int> map{};
    WHEN( "converting to a variant" )
    {
        auto v = Variant::from(map);
        THEN( "the variant is as expected" )
        {
            CHECK( v.is<Object>() );
            CHECK( v.as<Object>().empty() );
        }
    }
}
GIVEN( "a invalid variant object type" )
{
    Variant v(Object{{"a", 1},{"b", null}});
    WHEN( "converting to a std::unordered_map" )
    {
        using MapType = std::unordered_map<String, int>;
        CHECK_THROWS_AS( v.to<MapType>(), error::Conversion );
    }
}
}

#endif // #if CPPWAMP_TESTING_VARIANT
