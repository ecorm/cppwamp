/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#if CPPWAMP_TESTING_VARIANT

#include <algorithm>
#include <catch.hpp>
#include <cppwamp/types/set.hpp>
#include <cppwamp/types/unorderedmap.hpp>
#include <cppwamp/types/unorderedset.hpp>

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
            REQUIRE( v.is<Object>() );
            CHECK( v.as<Object>().empty() );
        }
    }
}
GIVEN( "an invalid variant object type" )
{
    Variant v(Object{{"a", 1},{"b", null}});
    WHEN( "converting to a std::unordered_map" )
    {
        using MapType = std::unordered_map<String, int>;
        CHECK_THROWS_AS( v.to<MapType>(), error::Conversion );
    }
}
}


//------------------------------------------------------------------------------
SCENARIO( "Converting to/from std::set", "[Variant]" )
{
GIVEN( "a valid variant array type" )
{
    Variant v(Array{1, 3, 2});
    WHEN( "converting to a std::set" )
    {
        auto set = v.to<std::set<int>>();
        THEN( "the set is as expected" )
        {
            REQUIRE( set.size() == 3 );
            auto iter = set.begin();
            CHECK( *iter++ == 1 );
            CHECK( *iter++ == 2 );
            CHECK( *iter++ == 3 );
        }
    }
}
GIVEN( "an empty variant array type" )
{
    Variant v(Array{});
    WHEN( "converting to a std::set" )
    {
        auto set = v.to<std::set<int>>();
        THEN( "the set is as expected" )
        {
            CHECK( set.empty() );
        }
    }
}
GIVEN( "a valid std::set type" )
{
    std::set<String> set{"a", "b", "c"};
    WHEN( "converting to a variant" )
    {
        auto v = Variant::from(set);
        THEN( "the variant is as expected" )
        {
            CHECK( v == (Array{"a", "b", "c"}) );
        }
    }
}
GIVEN( "an empty std::set" )
{
    std::set<String> set{};
    WHEN( "converting to a variant" )
    {
        auto v = Variant::from(set);
        THEN( "the variant is as expected" )
        {
            REQUIRE( v.is<Array>() );
            CHECK( v.as<Array>().empty() );
        }
    }
}
GIVEN( "an invalid variant array type" )
{
    Variant v(Array{"a", null});
    WHEN( "converting to a std::set" )
    {
        using SetType = std::set<String>;
        CHECK_THROWS_AS( v.to<SetType>(), error::Conversion );
    }
}
}


//------------------------------------------------------------------------------
SCENARIO( "Converting to/from std::unordered_set", "[Variant]" )
{
GIVEN( "a valid variant array type" )
{
    Variant v(Array{1, 3, 2});
    WHEN( "converting to a std::unordered_set" )
    {
        auto set = v.to<std::unordered_set<int>>();
        THEN( "the set is as expected" )
        {
            REQUIRE( set.size() == 3 );
            std::set<int> sorted(set.begin(), set.end());
            auto iter = sorted.begin();
            CHECK( *iter++ == 1 );
            CHECK( *iter++ == 2 );
            CHECK( *iter++ == 3 );
        }
    }
}
GIVEN( "an empty variant array type" )
{
    Variant v(Array{});
    WHEN( "converting to a std::unordered_set" )
    {
        auto set = v.to<std::unordered_set<int>>();
        THEN( "the set is as expected" )
        {
            CHECK( set.empty() );
        }
    }
}
GIVEN( "a valid std::unordered_set type" )
{
    std::unordered_set<String> set{"a", "b", "c"};
    WHEN( "converting to a variant" )
    {
        auto v = Variant::from(set);
        THEN( "the variant is as expected" )
        {
            auto array = v.as<Array>();
            std::sort(array.begin(), array.end());
            CHECK( array == (Array{"a", "b", "c"}) );
        }
    }
}
GIVEN( "an empty std::unordered_set" )
{
    std::unordered_set<String> set{};
    WHEN( "converting to a variant" )
    {
        auto v = Variant::from(set);
        THEN( "the variant is as expected" )
        {
            REQUIRE( v.is<Array>() );
            CHECK( v.as<Array>().empty() );
        }
    }
}
GIVEN( "an invalid variant array type" )
{
    Variant v(Array{"a", null});
    WHEN( "converting to a std::unordered_set" )
    {
        using SetType = std::unordered_set<String>;
        CHECK_THROWS_AS( v.to<SetType>(), error::Conversion );
    }
}
}

#endif // #if CPPWAMP_TESTING_VARIANT
