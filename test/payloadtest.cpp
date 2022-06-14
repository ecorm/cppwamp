/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#include <stdexcept>
#include <utility>
#include <catch2/catch.hpp>
#include <cppwamp/payload.hpp>
#include <cppwamp/internal/wampmessage.hpp>

using namespace wamp;

namespace
{

const Array testList{null, true, 42, "foo"};
Object testMap{{"a", null}, {"b", true}, {"c", 42}, {"d", "foo"}};

struct TestPayload : public Payload<TestPayload, internal::ResultMessage>
{};

} // anonymous namespace

//------------------------------------------------------------------------------
SCENARIO( "Initializing Payload", "[Variant][Payload]" )
{
    WHEN( "initializing from a list" )
    {
        auto p = TestPayload().withArgList(testList);
        CHECK( p.args() == testList );
        CHECK( p.kwargs().empty() );
    }

    WHEN( "initializing from a map" )
    {
        auto p = TestPayload().withKwargs(testMap);
        CHECK( p.args().empty() );
        CHECK( p.kwargs() == testMap );
    }

    WHEN( "initializing from a list and a map" )
    {
        auto p = TestPayload().withArgList(testList).withKwargs(testMap);
        CHECK( p.args() == testList );
        CHECK( p.kwargs() == testMap );
    }

    WHEN( "initializing from a parameter pack" )
    {
        auto p = TestPayload().withArgs(null, true, 42, "foo");
        CHECK( p.args() == testList );
    }
}

//------------------------------------------------------------------------------
SCENARIO( "Unbundling Payload to variables, with conversions",
          "[Variant][Payload]" )
{
GIVEN( "a Payload object and a set of variables" )
{
    const auto p = TestPayload().withArgList(testList);
    Null n;
    bool b = false;
    double x = 0.0;
    std::string s;

    WHEN( "unbundling positional values to valid variable types" )
    {
        CHECK( p.convertTo(n, b, x, s) == 4 );
        CHECK( n == testList.at(0) );
        CHECK( b == testList.at(1) );
        CHECK( x == testList.at(2) );
        CHECK( s == testList.at(3) );
    }

    WHEN( "unbundling positional values to too few variables" )
    {
        CHECK( p.convertTo(n, b, x) == 3 );
        CHECK( n == testList.at(0) );
        CHECK( b == testList.at(1) );
        CHECK( x == testList.at(2) );
    }

    WHEN( "unbundling positional values to extra variables" )
    {\
        int i = 42;
        CHECK( p.convertTo(n, b, x, s, i) == 4 );
        CHECK( n == testList.at(0) );
        CHECK( b == testList.at(1) );
        CHECK( x == testList.at(2) );
        CHECK( s == testList.at(3) );
        CHECK( i == 42 );
    }

    WHEN( "unbundling positional values to invalid variable types" )
    {
        int i = 0; // Invalid target type
        CHECK_THROWS_AS( p.convertTo(n, b, x, i), error::Conversion );
    }
}
}

//------------------------------------------------------------------------------
SCENARIO( "Moving Payload to variables, without conversion",
          "[Variant][Payload]" )
{
GIVEN( "a Payload object and a set of variables" )
{
    auto p = TestPayload().withArgList(testList);
    Null n;
    bool b = false;
    Int i = 0;
    double x = 0.0;
    std::string s;

    WHEN( "moving positional values to valid variable types" )
    {
        CHECK( p.moveTo(n, b, i, s) == 4 );
        CHECK( n == testList.at(0) );
        CHECK( b == testList.at(1) );
        CHECK( i == testList.at(2) );
        CHECK( s == testList.at(3) );
    }

    WHEN( "moving positional values to too few variables" )
    {
        CHECK( p.moveTo(n, b, i) == 3 );
        CHECK( n == testList.at(0) );
        CHECK( b == testList.at(1) );
        CHECK( i == testList.at(2) );
    }

    WHEN( "moving positional values to extra variables" )
    {
        x = 42;
        CHECK( p.moveTo(n, b, i, s, x) == 4 );
        CHECK( n == testList.at(0) );
        CHECK( b == testList.at(1) );
        CHECK( i == testList.at(2) );
        CHECK( s == testList.at(3) );
        CHECK( x == 42 );
    }

    WHEN( "moving positional values to invalid variable types" )
    {
        REQUIRE_THROWS_AS( p.moveTo(n, b, x, s), error::Access );
                          // Invalid type ^
    }
}
}

//------------------------------------------------------------------------------
SCENARIO( "Moving payload argument lists and maps", "[Variant][Payload]" )
{
GIVEN( "a Payload object with positional and keyword arguments" )
{
    auto p = TestPayload().withArgList(testList).withKwargs(testMap);

    WHEN( "moving positional arguments" )
    {
        Array list = std::move(p).args();
        CHECK( list == testList );
        CHECK( p.args().empty() );
        CHECK( p.kwargs() == testMap );
    }

    WHEN( "moving keyword arguments" )
    {
        Object map = std::move(p).kwargs();
        CHECK( map == testMap );
        CHECK( p.kwargs().empty() );
        CHECK( p.args() == testList  );
    }
}
}

//------------------------------------------------------------------------------
SCENARIO( "Indexing Payload elements", "[Variant][Payload]" )
{
GIVEN( "a Payload object with positional and keyword arguments" )
{
    auto p = TestPayload().withArgList(testList).withKwargs(testMap);

    WHEN( "indexing positional arguments" )
    {
        CHECK( p[0] == testList.at(0) );
        CHECK( p[1] == testList.at(1) );
        CHECK( p[2] == testList.at(2) );
        CHECK( p[3] == testList.at(3) );
        p[0] = "hello";
        CHECK( p[0] == "hello" );
        CHECK( p.args().at(0) == "hello" );
    }

    WHEN( "indexing positional arguments from a constant reference" )
    {
        const TestPayload& c = p;
        CHECK( c[0] == testList.at(0) );
        CHECK( c[1] == testList.at(1) );
        CHECK( c[2] == testList.at(2) );
        CHECK( c[3] == testList.at(3) );
    }

    WHEN( "indexing existing keyword arguments" )
    {
        CHECK( p["a"] == testMap["a"] );
        CHECK( p["b"] == testMap["b"] );
        CHECK( p["c"] == testMap["c"] );
        CHECK( p["d"] == testMap["d"] );
        p["a"] = "hello";
        CHECK( p["a"] == "hello" );
        Object kwargs = p.kwargs();
        CHECK( kwargs["a"] == "hello" );
    }

    WHEN( "indexing non-existent keyword arguments" )
    {
        p["e"] = 123.4;
        CHECK( p["e"] == 123.4 );
        Object kwargs = p.kwargs();
        CHECK( kwargs["e"] == 123.4 );
    }

    WHEN( "indexing out-of-range positional arguments" )
    {
        CHECK_THROWS_AS( p[5], std::out_of_range );
        CHECK_THROWS_AS( p[-1], std::out_of_range );
    }
}
}
