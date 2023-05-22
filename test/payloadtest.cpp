/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include <stdexcept>
#include <utility>
#include <catch2/catch.hpp>
#include <cppwamp/payload.hpp>
#include <cppwamp/internal/message.hpp>

// TODO: Test Options
// TODO: Test hasKwarg, kwargByKey, kwargOr, kwargAs

using namespace wamp;

namespace
{

const Array testList{null, true, 42, "foo"};
Object testMap{{"a", null}, {"b", true}, {"c", 42}, {"d", "foo"}};

//------------------------------------------------------------------------------
struct TestPayload : public Payload<TestPayload, internal::MessageKind::result>
{
    TestPayload() : Base(in_place, 0, Object{}) {}

private:
    using Base = Payload<TestPayload, internal::MessageKind::result>;
};

} // anonymous namespace

//------------------------------------------------------------------------------
SCENARIO( "Empty Payload", "[Variant][Payload]" )
{
    TestPayload p;
    CHECK( p.args().empty() );
    CHECK( p.kwargs().empty() );
    CHECK_FALSE( p.hasArgs() );
}

//------------------------------------------------------------------------------
SCENARIO( "Initializing Payload", "[Variant][Payload]" )
{
    WHEN( "initializing from a list" )
    {
        auto p = TestPayload().withArgList(testList);
        CHECK( p.args() == testList );
        CHECK( p.kwargs().empty() );
        CHECK( p.hasArgs() );
    }

    WHEN( "initializing from a map" )
    {
        auto p = TestPayload().withKwargs(testMap);
        CHECK( p.args().empty() );
        CHECK( p.kwargs() == testMap );
        CHECK( p.hasArgs() );
    }

    WHEN( "initializing from a list and a map" )
    {
        auto p = TestPayload().withArgList(testList).withKwargs(testMap);
        CHECK( p.args() == testList );
        CHECK( p.kwargs() == testMap );
        CHECK( p.hasArgs() );
    }

    WHEN( "initializing from a parameter pack" )
    {
        auto p = TestPayload().withArgs(null, true, 42, "foo");
        CHECK( p.args() == testList );
        CHECK( p.hasArgs() );
    }

    WHEN( "initializing from a tuple" )
    {
        std::tuple<Null, bool, int, std::string> tuple{null, true, 42, "foo"};
        auto p = TestPayload().withArgsTuple(tuple);
        CHECK( p.args() == testList );
        CHECK( p.hasArgs() );
    }
}

//------------------------------------------------------------------------------
SCENARIO( "Unbundling Payloads with conversions", "[Variant][Payload]" )
{
const auto p = TestPayload().withArgList(testList);

GIVEN( "a set of destination variables" )
{
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
GIVEN( "destination tuples" )
{
    std::tuple<Null, bool, double> t3;
    std::tuple<Null, bool, double, std::string> t4;
    std::tuple<Null, bool, double, std::string, int> t5;
    std::tuple<Null, bool, double, int> bad;

    WHEN( "unbundling positional values to valid element types" )
    {
        CHECK( p.convertToTuple(t4) == 4 );
        CHECK( std::get<0>(t4) == testList.at(0) );
        CHECK( std::get<1>(t4) == testList.at(1) );
        CHECK( std::get<2>(t4) == testList.at(2) );
        CHECK( std::get<3>(t4) == testList.at(3) );
    }

    WHEN( "unbundling positional values to too few elements" )
    {
        CHECK( p.convertToTuple(t3) == 3 );
        CHECK( std::get<0>(t3) == testList.at(0) );
        CHECK( std::get<1>(t3) == testList.at(1) );
        CHECK( std::get<2>(t3) == testList.at(2) );
    }

    WHEN( "unbundling positional values to extra elements" )
    {
        std::get<4>(t5) = 42;
        CHECK( p.convertToTuple(t5) == 4 );
        CHECK( std::get<0>(t5) == testList.at(0) );
        CHECK( std::get<1>(t5) == testList.at(1) );
        CHECK( std::get<2>(t5) == testList.at(2) );
        CHECK( std::get<3>(t5) == testList.at(3) );
        CHECK( std::get<4>(t5) == 42 );
    }

    WHEN( "unbundling positional values to invalid element types" )
    {
        CHECK_THROWS_AS( p.convertToTuple(bad), error::Conversion );
    }
}
}

//------------------------------------------------------------------------------
SCENARIO( "Moving Payloads without conversion", "[Variant][Payload]" )
{
auto p = TestPayload().withArgList(testList);

GIVEN( "a set of destination variables" )
{
    Null n;
    bool b = false;
    Int i = 0;
    Real x = 0.0;
    String s;

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

GIVEN( "destination tuples" )
{
    std::tuple<Null, bool, Int> t3;
    std::tuple<Null, bool, Int, String> t4;
    std::tuple<Null, bool, Int, String, Real> t5;
    std::tuple<Null, bool, Int, Real> bad;

    WHEN( "moving positional values to valid element types" )
    {
        CHECK( p.moveToTuple(t4) == 4 );
        CHECK( std::get<0>(t4) == testList.at(0) );
        CHECK( std::get<1>(t4) == testList.at(1) );
        CHECK( std::get<2>(t4) == testList.at(2) );
        CHECK( std::get<3>(t4) == testList.at(3) );
    }

    WHEN( "moving positional values to too few elements" )
    {
        CHECK( p.moveToTuple(t3) == 3 );
        CHECK( std::get<0>(t3) == testList.at(0) );
        CHECK( std::get<1>(t3) == testList.at(1) );
        CHECK( std::get<2>(t3) == testList.at(2) );
    }

    WHEN( "moving positional values to extra variables" )
    {
        std::get<4>(t5) = 42;
        CHECK( p.moveToTuple(t5) == 4 );
        CHECK( std::get<0>(t5) == testList.at(0) );
        CHECK( std::get<1>(t5) == testList.at(1) );
        CHECK( std::get<2>(t5) == testList.at(2) );
        CHECK( std::get<3>(t5) == testList.at(3) );
        CHECK( std::get<4>(t5) == 42 );
    }

    WHEN( "moving positional values to invalid variable types" )
    {
        REQUIRE_THROWS_AS( p.moveToTuple(bad), error::Access );
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
