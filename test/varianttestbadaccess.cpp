/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#if CPPWAMP_TESTING_VARIANT

#include <cstdlib>
#include <limits>
#include <type_traits>
#include <catch.hpp>
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
SCENARIO( "Variant bad access", "[Variant]" )
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

#endif // #if CPPWAMP_TESTING_VARIANT
