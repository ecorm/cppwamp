/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#if CPPWAMP_TESTING_WAMP

#include <sstream>
#include <catch.hpp>
#include <cppwamp/args.hpp>

using namespace wamp;

namespace
{

const Array testList{null, true, 42, "foo"};
Object testMap{{"a", null}, {"b", true}, {"c", 42}, {"d", "foo"}};

bool checkOutput(const Args& args, const std::string& expected)
{
    std::ostringstream oss;
    oss << args;
    CHECK( oss.str() == expected );
    return oss.str() == expected;
}

} // anonymous namespace

//------------------------------------------------------------------------------
SCENARIO( "Initializing Args", "[Args]" )
{
    WHEN( "initializing from a list" )
    {
        Args args(with, testList);
        CHECK(( args.list == testList ));
        CHECK( args.map.empty() );
    }

    WHEN( "initializing from a map" )
    {
        Args args(with, testMap);
        CHECK( args.list.empty() );
        CHECK( args.map == testMap );
    }

    WHEN( "initializing from a list and a map" )
    {
        Args args(with, testList, testMap);
        CHECK( args.list == testList );
        CHECK( args.map == testMap );
    }

    WHEN( "initializing from a braced initializer list" )
    {
        Args args{null, true, 42, "foo"};
        CHECK( args.list == testList );
        CHECK( args.map.empty() );
    }

    WHEN( "initializing from a braced initializer list of pairs" )
    {
        Args args{ withPairs, {{"a",null}, {"b",true}, {"c",42}, {"d","foo"}} };
        CHECK( args.list.empty() );
        CHECK( args.map["a"].as<Null>() == null );
        CHECK( args.map["b"].as<Bool>() == true );
        CHECK( args.map["c"].as<Int>() == 42 );
        CHECK_THAT( args.map["d"].as<String>(), Equals("foo") );
    }
}

//------------------------------------------------------------------------------
SCENARIO( "Unbundling Args to variables, with conversions", "[Args]" )
{
GIVEN( "an Args object and a set of variables" )
{
    const Args args(with, testList);
    Null n;
    bool b = false;
    double x = 0.0;
    std::string s;

    WHEN( "unbundling positional values to valid variable types" )
    {
        CHECK( args.to(n, b, x, s) == 4 );
        CHECK( n == testList.at(0) );
        CHECK( b == testList.at(1) );
        CHECK( x == testList.at(2) );
        CHECK( s == testList.at(3) );
    }

    WHEN( "unbundling positional values to too few variables" )
    {\
        s = "foo";
        CHECK( args.to(n, b, x) == 3 );
        CHECK( n == testList.at(0) );
        CHECK( b == testList.at(1) );
        CHECK( x == testList.at(2) );
        CHECK_THAT( s, Equals("foo") );
    }

    WHEN( "unbundling positional values to extra variables" )
    {\
        int i = 42;
        CHECK( args.to(n, b, x, s, i) == 4 );
        CHECK( n == testList.at(0) );
        CHECK( b == testList.at(1) );
        CHECK( x == testList.at(2) );
        CHECK( s == testList.at(3) );
        CHECK( i == 42 );
    }

    WHEN( "unbundling positional values to invalid variable types" )
    {
        int i = 0;
        CHECK_THROWS_AS( args.to(n, b, x, i), error::Conversion );
                          // Invalid type ^
    }
}
}

//------------------------------------------------------------------------------
SCENARIO( "Moving Args to variables, without conversion", "[Args]" )
{
GIVEN( "an Args object and a set of variables" )
{
    Args args(with, testList);
    Null n;
    bool b = false;
    Int i = 0;
    double x = 0.0;
    std::string s;

    WHEN( "moving positional values to valid variable types" )
    {
        CHECK( args.move(n, b, i, s) == 4 );
        CHECK( n == testList.at(0) );
        CHECK( b == testList.at(1) );
        CHECK( i == testList.at(2) );
        CHECK( s == testList.at(3) );
    }

    WHEN( "moving positional values to too few variables" )
    {
        s = "foo";
        CHECK( args.move(n, b, i) == 3 );
        CHECK( n == testList.at(0) );
        CHECK( b == testList.at(1) );
        CHECK( i == testList.at(2) );
        CHECK_THAT( s, Equals("foo") );
    }

    WHEN( "moving positional values to extra variables" )
    {
        x = 42;
        CHECK( args.move(n, b, i, s, x) == 4 );
        CHECK( n == testList.at(0) );
        CHECK( b == testList.at(1) );
        CHECK( i == testList.at(2) );
        CHECK( s == testList.at(3) );
        CHECK( x == 42 );
    }

    WHEN( "moving positional values to invalid variable types" )
    {
        REQUIRE_THROWS_AS( args.move(n, b, x, s), error::Access );
                           // Invalid type ^
    }
}
}

//------------------------------------------------------------------------------
SCENARIO( "Indexing Args elements", "[Args]" )
{
GIVEN( "an Args object with positional and keyword arguments" )
{
    Args args(with, testList, testMap);

    WHEN( "indexing positional arguments" )
    {
        CHECK( args[0] == testList.at(0) );
        CHECK( args[1] == testList.at(1) );
        CHECK( args[2] == testList.at(2) );
        CHECK( args[3] == testList.at(3) );
        args[0] = "hello";
        CHECK( args.list.at(0) == "hello" );
    }

    WHEN( "indexing positional arguments from a constant reference" )
    {
        const Args& cargs = args;
        CHECK( cargs[0] == testList.at(0) );
        CHECK( cargs[1] == testList.at(1) );
        CHECK( cargs[2] == testList.at(2) );
        CHECK( cargs[3] == testList.at(3) );
    }

    WHEN( "indexing existing keyword arguments" )
    {
        CHECK( args["a"] == testMap["a"] );
        CHECK( args["b"] == testMap["b"] );
        CHECK( args["c"] == testMap["c"] );
        CHECK( args["d"] == testMap["d"] );
        args["a"] = "hello";
        CHECK( args.map["a"] == "hello" );
    }

    WHEN( "indexing non-existing keyword arguments" )
    {
        args["e"] = 123.4;
        CHECK( args["e"] == 123.4 );
        CHECK( args.map["e"] == 123.4 );
    }

    WHEN( "indexing out-of-range positional arguments" )
    {
        CHECK_THROWS_AS( args[5], std::out_of_range );
        CHECK_THROWS_AS( args[-1], std::out_of_range );
    }
}
}

//------------------------------------------------------------------------------
SCENARIO( "Comparing Args", "[Args]" )
{
GIVEN( "an empty Args object" )
{
    Args args;

    WHEN( "comparing to another empty Args object" )
    {
        Args other;
        CHECK( args == other );
    }

    WHEN( "comparing to non-empty Args objects" )
    {
        CHECK( args != (Args{ with, Array{ null } }) );
        CHECK( args != (Args{ with, Object{ {"", null} } }) );
        CHECK( args != (Args{ with, Array{ null }, Object{ {"", null} } }) );
    }
}
GIVEN( "an Args object with only positional arguments" )
{
    Args args{null, true, 42, "foo"};

    WHEN( "comparing to equivalent Args objects" )
    {
        CHECK(       args == (Args{null, true, 42, "foo"}) );
        CHECK_FALSE( args != (Args{null, true, 42, "foo"}) );
        CHECK(       args == (Args{null, true, 42.0, "foo"}) );
        CHECK_FALSE( args != (Args{null, true, 42.0, "foo"}) );
    }

    WHEN( "comparing to different Args with only positional arguments" )
    {
        CHECK(       args != (Args{null, true, 42, "fo"}) );
        CHECK_FALSE( args == (Args{null, true, 42, "fo"}) );
        CHECK(       args != (Args{null, 1, 42, "foo"}) );
        CHECK_FALSE( args == (Args{null, 1, 42, "foo"}) );
        CHECK(       args != (Args{null, true, 42}) );
        CHECK_FALSE( args == (Args{null, true, 42}) );
    }

    WHEN( "comparing to Args with only keyword arguments" )
    {
        Args other{with, Object{{"", null}, {"", true}, {"", 42}, {"", "foo"}}};
        CHECK(       args != other );
        CHECK_FALSE( args == other );
    }

    WHEN( "comparing to Args with both argument list and map" )
    {
        Args other(args);
        other.map[""] = null;
        CHECK(       args != other );
        CHECK_FALSE( args == other );
    }
}
GIVEN( "an Args object with only keyword arguments" )
{
    Args args{with, Object{{"a", null}, {"b", true}, {"c", 42}, {"d", "foo"}} };

    WHEN( "comparing to equivalent Args objects" )
    {
        Args other = args;
        CHECK(       args == other );
        CHECK_FALSE( args != other );
        other["c"] = 42.0;
        CHECK(       args == other );
        CHECK_FALSE( args != other );
    }

    WHEN( "comparing to different Args with only keyword arguments" )
    {
        CHECK(       args != Args(with, Object{ {"a", null}, {"b", 1},
                                                {"c", 42}, {"d", "foo"} }) );
        CHECK_FALSE( args == Args(with, Object{ {"a", null}, {"b", 1},
                                                {"c", 42}, {"d", "foo"} }) );
        CHECK(       args != Args(with, Object{ {"a", null}, {"b", true},
                                                {"c", 42}, {"D", "foo"} }) );
        CHECK_FALSE( args == Args(with, Object{ {"a", null}, {"b", true},
                                                {"c", 42}, {"D", "foo"} }) );
        CHECK(       args != Args(with, Object{ {"a", null}, {"b", true},
                                                {"c", 42} }) );
        CHECK_FALSE( args == Args(with, Object{ {"a", null}, {"b", true},
                                                {"c", 42} }) );
    }

    WHEN( "comparing to Args with only positional arguments" )
    {
        CHECK(       args != (Args{null, true, 42, "foo"}) );
        CHECK_FALSE( args == (Args{null, true, 42, "foo"}) );
    }

    WHEN( "comparing to Args with both argument list and map" )
    {
        Args other(args);
        other.list.push_back(null);
        CHECK(       args != other );
        CHECK_FALSE( args == other );
    }
}
}

//------------------------------------------------------------------------------
SCENARIO( "Outputting args", "[Args]" )
{
GIVEN( "an assortment of args" )
{
    CHECK( checkOutput(Args{}, R"(Args{[],{}})") );
    CHECK( checkOutput(Args{null}, R"(Args{[null],{}})") );
    CHECK( checkOutput(Args{null, true, 42, "foo"},
                       R"(Args{[null,true,42,"foo"],{}})") );
    CHECK( checkOutput(Args{Array{}}, R"(Args{[[]],{}})") );
    CHECK( checkOutput(Args{Array{"foo"}}, R"(Args{[["foo"]],{}})") );
    CHECK( checkOutput(Args{Object{}}, R"(Args{[{}],{}})") );
    CHECK( checkOutput(Args{Object{{"foo",42}}}, R"(Args{[{"foo":42}],{}})") );
    CHECK( checkOutput(Args{with, Object{{"", null}}},
                       R"(Args{[],{"":null}})") );
    CHECK( checkOutput(Args{with, testMap},
                       R"(Args{[],{"a":null,"b":true,"c":42,"d":"foo"}})" ) );
    CHECK( checkOutput(Args{with, testList, testMap},
       R"(Args{[null,true,42,"foo"],{"a":null,"b":true,"c":42,"d":"foo"}})" ) );
}
}


#endif // #if CPPWAMP_TESTING_WAMP

