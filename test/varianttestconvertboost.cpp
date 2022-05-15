/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#include <catch2/catch.hpp>
#include <boost/optional/optional_io.hpp>
#include <cppwamp/types/boostoptional.hpp>

using namespace wamp;

namespace
{

template <typename T>
void checkOptional(const T& value)
{
    INFO( "For value " << value );
    Variant v(value);
    boost::optional<T> o;
    CHECK_NOTHROW( v.to(o) );
    CHECK( !!o );
    CHECK(  *o == value );
    CHECK( v == value );
}

template <typename T>
void checkSame(const Variant& v, const boost::optional<T>& o)
{
    INFO( "For variant = " << v << " and optional = " << o );
    CHECK( v == o );
    CHECK( o == v );
    CHECK( !(v != o) );
    CHECK( !(o != v) );
}

template <typename T>
void checkDifferent(const Variant& v, const boost::optional<T>& o)
{
    INFO( "For variant = " << v << " and optional = " << o );
    CHECK( v != o );
    CHECK( o != v );
    CHECK( !(v == o) );
    CHECK( !(o == v) );
}

} // anonymous namespace


//------------------------------------------------------------------------------
SCENARIO( "Converting to/from Boost.Optional", "[Variant]" )
{
GIVEN( "an empty Boost.Optional" )
{
    boost::optional<int> opt;

    WHEN( "converted to a variant" )
    {
        auto v = Variant::from(opt);

        THEN( "the variant is null" )
        {
            CHECK( !v );
        }
    }
}
GIVEN( "a null variant" )
{
    Variant v;

    WHEN( "converted to a boost optional" )
    {
        boost::optional<int> opt;
        v.to(opt);

        THEN( "the optional is empty" )
        {
            CHECK( !opt );
        }
    }
}
GIVEN( "an assortment of Boost.Optional types" )
{
    checkOptional(false);
    checkOptional(true);
    checkOptional(42u);
    checkOptional(-123);
    checkOptional(3.1415);
    checkOptional(String("foo"));
}
GIVEN( "an invalid variant type" )
{
    Variant v("foo");
    WHEN( "converting to an incompatible boost::optional" )
    {
        boost::optional<int> opt;
        CHECK_THROWS_AS( v.to(opt), error::Conversion );
    }
}
}

//------------------------------------------------------------------------------
SCENARIO( "Comparing variants with Boost.Optional", "[Variant]" )
{
GIVEN( "an empty optional" )
{
    boost::optional<int> opt;
    checkSame( Variant(null), opt );
    checkDifferent( Variant(true), opt );
    checkDifferent( Variant(0), opt );
}
GIVEN( "a null variant" )
{
    Variant v;
    using boost::optional;
    checkSame( v, optional<int>() );
    checkDifferent( v, optional<bool>(false) );
}
GIVEN( "non empty variants and optionals" )
{
    using boost::optional;
    checkSame( Variant(42), optional<int>(42) );
    checkSame( Variant(42), optional<float>(42.0f) );
    checkSame( Variant(42.0), optional<int>(42) );
    checkDifferent( Variant("42"), optional<int>(42) );
    checkDifferent( Variant("42"), optional<String>("") );
}
}
