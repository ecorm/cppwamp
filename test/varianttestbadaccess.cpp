/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include <cstdlib>
#include <limits>
#include <type_traits>
#include <catch2/catch.hpp>
#include <cppwamp/variant.hpp>

using namespace wamp;


namespace
{

template <typename T>
void checkBadAccess(const T& value)
{
    Variant v(value);
    const Variant& cv = v;
    INFO( "For variant of type '" << typeNameOf(v) <<
          "' and value '" << v << "'");
    if (!v.is<Null>())
    {
        CHECK_THROWS_AS( v.as<Null>(), error::Access );
        CHECK_THROWS_AS( cv.as<Null>(), error::Access );
        CHECK_THROWS_AS( v.as<TypeId::null>(), error::Access );
        CHECK_THROWS_AS( cv.as<TypeId::null>(), error::Access );
    }
    if (!v.is<Bool>())
    {
        CHECK_THROWS_AS( v.as<Bool>(), error::Access );
        CHECK_THROWS_AS( cv.as<Bool>(), error::Access );
        CHECK_THROWS_AS( v.as<TypeId::boolean>(), error::Access );
        CHECK_THROWS_AS( cv.as<TypeId::boolean>(), error::Access );
    }
    if (!v.is<Int>())
    {
        CHECK_THROWS_AS( v.as<Int>(), error::Access );
        CHECK_THROWS_AS( cv.as<Int>(), error::Access );
        CHECK_THROWS_AS( v.as<TypeId::integer>(), error::Access );
        CHECK_THROWS_AS( cv.as<TypeId::integer>(), error::Access );
    }
    if (!v.is<UInt>())
    {
        CHECK_THROWS_AS( v.as<UInt>(), error::Access );
        CHECK_THROWS_AS( cv.as<UInt>(), error::Access );
        CHECK_THROWS_AS( v.as<TypeId::uint>(), error::Access );
        CHECK_THROWS_AS( cv.as<TypeId::uint>(), error::Access );
    }
    if (!v.is<Real>())
    {
        CHECK_THROWS_AS( v.as<Real>(), error::Access );
        CHECK_THROWS_AS( cv.as<Real>(), error::Access );
        CHECK_THROWS_AS( v.as<TypeId::real>(), error::Access );
        CHECK_THROWS_AS( cv.as<TypeId::real>(), error::Access );
    }
    if (!v.is<String>())
    {
        CHECK_THROWS_AS( v.as<String>(), error::Access );
        CHECK_THROWS_AS( cv.as<String>(), error::Access );
        CHECK_THROWS_AS( v.as<TypeId::string>(), error::Access );
        CHECK_THROWS_AS( cv.as<TypeId::string>(), error::Access );
    }
    if (!v.is<Blob>())
    {
        CHECK_THROWS_AS( v.as<Blob>(), error::Access );
        CHECK_THROWS_AS( cv.as<Blob>(), error::Access );
        CHECK_THROWS_AS( v.as<TypeId::blob>(), error::Access );
        CHECK_THROWS_AS( cv.as<TypeId::blob>(), error::Access );
    }
    if (!v.is<Array>())
    {
        CHECK_THROWS_AS( v.as<Array>(), error::Access );
        CHECK_THROWS_AS( cv.as<Array>(), error::Access );
        CHECK_THROWS_AS( v.as<TypeId::array>(), error::Access );
        CHECK_THROWS_AS( cv.as<TypeId::array>(), error::Access );
    }
    if (!v.is<Object>())
    {
        CHECK_THROWS_AS( v.as<Object>(), error::Access );
        CHECK_THROWS_AS( cv.as<Object>(), error::Access );
        CHECK_THROWS_AS( v.as<TypeId::object>(), error::Access );
        CHECK_THROWS_AS( cv.as<TypeId::object>(), error::Access );
    }
}

} // anonymous namespace


//------------------------------------------------------------------------------
SCENARIO( "Variant bad type access", "[Variant]" )
{
GIVEN( "assorted Variants" )
{
    auto intMin  = std::numeric_limits<Int>::min();
    auto intMax  = std::numeric_limits<Int>::max();
    auto uintMax = std::numeric_limits<UInt>::max();
    auto realMin = std::numeric_limits<Real>::lowest();
    auto realMax = std::numeric_limits<Real>::max();

    checkBadAccess(null);
    checkBadAccess(true);
    checkBadAccess(false);
    checkBadAccess(0);
    checkBadAccess(intMin);
    checkBadAccess(intMax);
    checkBadAccess(0u);
    checkBadAccess(uintMax);
    checkBadAccess(0.0);
    checkBadAccess(realMin);
    checkBadAccess(realMax);
    checkBadAccess("");
    checkBadAccess("null");
    checkBadAccess("true");
    checkBadAccess("false");
    checkBadAccess("0");
    checkBadAccess("1");
    checkBadAccess(Blob{});
    checkBadAccess(Blob{0x00});
    checkBadAccess(Blob{0x00, 0x01, 0x02});
    checkBadAccess(Array{});
    checkBadAccess(Array{null});
    checkBadAccess(Array{true});
    checkBadAccess(Array{false});
    checkBadAccess(Array{0});
    checkBadAccess(Array{0u});
    checkBadAccess(Array{0.0});
    checkBadAccess(Array{""});
    checkBadAccess(Array{Array{}});
    checkBadAccess(Array{Object{}});
    checkBadAccess(Object{ {"",null} });
    checkBadAccess(Object{ {"",true} });
    checkBadAccess(Object{ {"",false} });
    checkBadAccess(Object{ {"",0} });
    checkBadAccess(Object{ {"",0u} });
    checkBadAccess(Object{ {"",0.0} });
    checkBadAccess(Object{ {"",""} });
    checkBadAccess(Object{ {"",Array{}} });
    checkBadAccess(Object{ {"",Object{}} });
}
}

//------------------------------------------------------------------------------
SCENARIO( "Variant bad index access", "[Variant]" )
{
GIVEN( "a non-composite variant" )
{
    Variant v(42);

    WHEN( "accessing an element by index" )
    {
        CHECK_THROWS_AS( v[0], error::Access );
        CHECK_THROWS_AS( v.at(0), error::Access );
    }
    WHEN( "accessing an element by key" )
    {
        CHECK_THROWS_AS( v["foo"], error::Access );
        CHECK_THROWS_AS( v.at("foo"), error::Access );
    }
}
GIVEN( "an array variant" )
{
    Variant v(Array{42, "foo"});

    WHEN( "accessing out of range" )
    {
        CHECK_THROWS_AS( v[2], std::out_of_range );
        CHECK_THROWS_AS( v.at(2), std::out_of_range );
    }
    WHEN( "accessing an element by key" )
    {
        CHECK_THROWS_AS( v["foo"], error::Access );
        CHECK_THROWS_AS( v.at("foo"), error::Access );
    }
}
GIVEN( "an object variant" )
{
    Variant v(Object{{"0", true}});

    WHEN( "accessing an element by index" )
    {
        CHECK_THROWS_AS( v[0], error::Access );
        CHECK_THROWS_AS( v.at(0), error::Access );
    }
    WHEN( "accessing a non-exitent element using operator[]" )
    {
        auto& elem = v["foo"];
        THEN( "a null element is automatically inserted" )
        {
            CHECK( v.size() == 2 );
            CHECK( elem.is<Null>() );
        }
    }
    WHEN( "accessing a non-exitent element using at()" )
    {
        CHECK_THROWS_AS( v.at("foo"), std::out_of_range );
    }
}
}
