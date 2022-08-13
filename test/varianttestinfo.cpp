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
using namespace Catch::Matchers;

//------------------------------------------------------------------------------
SCENARIO( "Variant type information", "[Variant]" )
{
    using I = TypeId;
    GIVEN( "a default-constructed Variant" )
    {
        Variant v;
        THEN( "The type information is as expected" )
        {
            CHECK( (v.typeId() == I::null) );
            CHECK_THAT( typeNameOf(v), Equals("Null") );
            CHECK( !v );
            CHECK( v.is<Null>() );
            CHECK( v.is<I::null>() );
            CHECK( v.size() == 0 );
            CHECK( !isNumber(v) );
            CHECK( !isScalar(v) );
        }
    }
    GIVEN( "a Null Variant" )
    {
        Variant v(null);
        THEN( "The type information is as expected" )
        {
            CHECK( (v.typeId() == I::null) );
            CHECK_THAT( typeNameOf(v), Equals("Null") );
            CHECK( !v );
            CHECK( v.is<Null>() );
            CHECK( v.is<I::null>() );
            CHECK( v.size() == 0 );
            CHECK( !isNumber(v) );
            CHECK( !isScalar(v) );
        }
    }
    GIVEN( "a Bool Variant" )
    {
        Variant v(true);
        THEN( "The type information is as expected" )
        {
            CHECK( (v.typeId() == I::boolean) );
            CHECK_THAT( typeNameOf(v), Equals("Bool") );
            CHECK( !!v );
            CHECK( v.is<Bool>() );
            CHECK( v.is<I::boolean>() );
            CHECK( v.size() == 1 );
            CHECK( !isNumber(v) );
            CHECK( isScalar(v) );
        }
    }
    GIVEN( "an Int Variant" )
    {
        Variant v(Int(-42));
        THEN( "The type information is as expected" )
        {
            CHECK( (v.typeId() == I::integer) );
            CHECK_THAT( typeNameOf(v), Equals("Int") );
            CHECK( !!v );
            CHECK( v.is<Int>() );
            CHECK( v.is<I::integer>() );
            CHECK( v.size() == 1 );
            CHECK( isNumber(v) );
            CHECK( isScalar(v) );
        }
    }
    GIVEN( "an UInt Variant" )
    {
        Variant v(UInt(42u));
        THEN( "The type information is as expected" )
        {
            CHECK( (v.typeId() == I::uint) );
            CHECK_THAT( typeNameOf(v), Equals("UInt") );
            CHECK( !!v );
            CHECK( v.is<UInt>() );
            CHECK( v.is<I::uint>() );
            CHECK( v.size() == 1 );
            CHECK( isNumber(v) );
            CHECK( isScalar(v) );
        }
    }
    GIVEN( "a Real Variant" )
    {
        Variant v(Real(42.0));
        THEN( "The type information is as expected" )
        {
            CHECK( (v.typeId() == I::real) );
            CHECK_THAT( typeNameOf(v), Equals("Real") );
            CHECK( !!v );
            CHECK( v.is<Real>() );
            CHECK( v.is<I::real>() );
            CHECK( v.size() == 1 );
            CHECK( isNumber(v) );
            CHECK( isScalar(v) );
        }
    }
    GIVEN( "a String Variant" )
    {
        Variant v(String("Hello"));
        THEN( "The type information is as expected" )
        {
            CHECK( (v.typeId() == I::string) );
            CHECK_THAT( typeNameOf(v), Equals("String") );
            CHECK( !!v );
            CHECK( v.is<String>() );
            CHECK( v.is<I::string>() );
            CHECK( v.size() == 1 );
            CHECK( !isNumber(v) );
            CHECK( !isScalar(v) );
        }
    }
    GIVEN( "a Blob Variant" )
    {
        Variant v(Blob{0x00, 0x01, 0x02});
        THEN( "The type information is as expected" )
        {
            CHECK( (v.typeId() == I::blob) );
            CHECK_THAT( typeNameOf(v), Equals("Blob") );
            CHECK( !!v );
            CHECK( v.is<Blob>() );
            CHECK( v.is<I::blob>() );
            CHECK( v.size() == 1 );
            CHECK( !isNumber(v) );
            CHECK( !isScalar(v) );
        }
    }
    GIVEN( "an Array Variant" )
    {
        Variant v(Array{42, "hello", false});
        THEN( "The type information is as expected" )
        {
            CHECK( (v.typeId() == I::array) );
            CHECK_THAT( typeNameOf(v), Equals("Array") );
            CHECK( !!v );
            CHECK( v.is<Array>() );
            CHECK( v.is<I::array>() );
            CHECK( v.size() == 3 );
            CHECK( !isNumber(v) );
            CHECK( !isScalar(v) );
        }
    }
    GIVEN( "an Object Variant" )
    {
        Variant v(Object{{"foo",42}, {"bar","hello"}});
        THEN( "The type information is as expected" )
        {
            CHECK( (v.typeId() == I::object) );
            CHECK_THAT( typeNameOf(v), Equals("Object") );
            CHECK( !!v );
            CHECK( v.is<Object>() );
            CHECK( v.is<I::object>() );
            CHECK( v.size() == 2 );
            CHECK( !isNumber(v) );
            CHECK( !isScalar(v) );
        }
    }
}
