/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#if CPPWAMP_TESTING_VARIANT

#include <tuple>
#include <catch.hpp>
#include <cppwamp/variant.hpp>
#include <cppwamp/varianttuple.hpp>

using namespace wamp;

//------------------------------------------------------------------------------
SCENARIO( "Variant initialization from a tuple", "[Variant]" )
{
GIVEN( "a tuple of valid types" )
{
    auto tuple = std::make_tuple(null, false, true, 0u, -1, 42.0, "foo",
                                 Array{"a", 123}, Object{{"o", 321}});
    Variant expected = Array{null, false, true, 0u, -1, 42.0, "foo",
                             Array{"a", 123}, Object{{"o", 321}}};
    WHEN( "a variant is constructed with the tuple" )
    {
        Variant v(toArray(tuple));
        CHECK( v == expected );
    }
    WHEN( "the tuple is assigned to a variant" )
    {
        Variant v;
        v = toArray(tuple);
        CHECK( v == expected );
    }
}
GIVEN( "an empty tuple" )
{
    std::tuple<> tuple;
    Variant expected = Array{};
    WHEN( "a variant is constructed with the tuple" )
    {
        Variant v(toArray(tuple));
        CHECK( v == expected );
    }
    WHEN( "the tuple is assigned to a variant" )
    {
        Variant v;
        v = toArray(tuple);
        CHECK( v == expected );
    }
}
// The following should cause 4 static assertion failures:
//GIVEN( "a tuple with an invalid type" )
//{
//    struct Invalid {};
//    auto tuple = std::make_tuple(true, 42.0, Invalid());
//    Array array;
//    array = toArray(tuple);
//    toTuple(array, tuple);
//    bool equal = array == tuple;
//    bool notequal = array != tuple;
//}
}

SCENARIO( "Variant conversion/comparison to tuple", "[Variant]" )
{
    GIVEN( "a tuple of valid types" )
    {
        auto tuple = std::make_tuple(null, false, true, 0u, -1, 42.0,
                String("foo"), Array{"a", 123}, Object{{"o", 321}});
        using TupleType = decltype(tuple);

        WHEN( "a matching variant is converted to the tuple" )
        {
            Variant v(toArray(tuple));
            TupleType result;
            CHECK( convertsToTuple(v.as<Array>(), tuple) );
            REQUIRE_NOTHROW( toTuple(v.as<Array>(), result) );
            CHECK(( result == tuple ));
            CHECK(( v == tuple ));
            CHECK(( v.as<Array>() == tuple ));
            CHECK_FALSE(( v != tuple ));
            CHECK_FALSE(( v.as<Array>() != tuple ));
        }
        WHEN( "a matching variant differs by only one value" )
        {
            Variant v(toArray(tuple));
            v.as<Array>().at(3).as<UInt>() = 666u;
            CHECK_FALSE(( v == tuple ));
            CHECK_FALSE(( v.as<Array>() == tuple ));
            CHECK(( v != tuple ));
            CHECK(( v.as<Array>() != tuple ));
        }
    }
    GIVEN( "a tuple of convertible types" )
    {
        struct Bogus{};
        auto tuple = std::make_tuple(false, 3, 42.0, String("123"));
        using TupleType = decltype(tuple);

        WHEN( "a compatible variant is converted to the tuple" )
        {
            Variant v(Array{0, 3u, 42, String("123")});
            TupleType result;
            CHECK( convertsToTuple(v.as<Array>(), tuple) );
            REQUIRE_NOTHROW( toTuple(v.as<Array>(), result) );
            CHECK(( result == tuple ));
        }
        WHEN( "a compatible variant is compared to the tuple" )
        {
            Variant v(Array{false, 3u, 42, "123"});
            CHECK(( v == tuple ));
            CHECK(( v.as<Array>() == tuple ));
            CHECK_FALSE(( v != tuple ));
            CHECK_FALSE(( v.as<Array>() != tuple ));
        }
        WHEN( "a compatible variant differs by only one value" )
        {
            Variant v(Array{false, 3u, 41, "123"});
            CHECK_FALSE(( v == tuple ));
            CHECK_FALSE(( v.as<Array>() == tuple ));
            CHECK(( v != tuple ));
            CHECK(( v.as<Array>() != tuple ));
        }
    }
    GIVEN( "an empty tuple" )
    {
        std::tuple<> tuple;
        using TupleType = decltype(tuple);

        WHEN( "an empty array variant is converted to the tuple" )
        {
            Variant v(Array{});
            TupleType result;
            CHECK( convertsToTuple(v.as<Array>(), tuple) );
            REQUIRE_NOTHROW( toTuple(v.as<Array>(), result) );
            CHECK(( result == tuple ));
            CHECK(( v == tuple ));
            CHECK(( v.as<Array>() == tuple ));
            CHECK_FALSE(( v != tuple ));
            CHECK_FALSE(( v.as<Array>() != tuple ));
        }
        WHEN( "a non-empty array variant is converted to the tuple" )
        {
            Variant v(Array{null});
            TupleType result;
            CHECK_FALSE( convertsToTuple(v.as<Array>(), tuple) );
            REQUIRE_THROWS_AS( toTuple(v.as<Array>(), result),
                               error::Conversion );
            CHECK_FALSE(( v == tuple ));
            CHECK_FALSE(( v.as<Array>() == tuple ));
            CHECK(( v != tuple ));
            CHECK(( v.as<Array>() != tuple ));
        }
        WHEN( "a null variant is compared to the tuple" )
        {
            Variant v;
            CHECK_FALSE(( v == tuple ));
            CHECK(( v != tuple ));
        }
    }
    GIVEN( "a wrongly-sized tuple type" )
    {
        auto tuple = std::make_tuple(true, Int(42));
        using TupleType = decltype(tuple);

        WHEN( "an array Variant is narrower than the tuple" )
        {
            Variant v(Array{true});
            TupleType result;
            CHECK_FALSE( convertsToTuple(v.as<Array>(), tuple) );
            REQUIRE_THROWS_AS( toTuple(v.as<Array>(), result),
                               error::Conversion );
            CHECK_FALSE(( v == tuple ));
            CHECK_FALSE(( v.as<Array>() == tuple ));
            CHECK(( v != tuple ));
            CHECK(( v.as<Array>() != tuple ));
        }
        WHEN( "an array Variant is wider than the tuple" )
        {
            Variant v(Array{true, 42, null});
            TupleType result;
            CHECK_FALSE( convertsToTuple(v.as<Array>(), tuple) );
            REQUIRE_THROWS_AS( toTuple(v.as<Array>(), result),
                               error::Conversion );
            CHECK_FALSE(( v == tuple ));
            CHECK_FALSE(( v.as<Array>() == tuple ));
            CHECK(( v != tuple ));
            CHECK(( v.as<Array>() != tuple ));
        }
    }
    GIVEN( "a correctly-sized tuple with unconvertible types" )
    {
        auto tuple = std::make_tuple(null, true, Int(42));
        using TupleType = decltype(tuple);

        WHEN( "a Variant is converted to a mismatched tuple" )
        {
            Variant v(Array{true, null, 42});
            TupleType result;
            CHECK_FALSE( convertsToTuple(v.as<Array>(), tuple) );
            REQUIRE_THROWS_AS( toTuple(v.as<Array>(), result),
                               error::Conversion );
            CHECK_FALSE(( v == tuple ));
            CHECK_FALSE(( v.as<Array>() == tuple ));
            CHECK(( v != tuple ));
            CHECK(( v.as<Array>() != tuple ));
        }
    }
}

#endif // #if CPPWAMP_TESTING_VARIANT
