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
            CHECK( !isNumber(v) );
            CHECK( !isScalar(v) );
        }
    }
}

#endif // #if CPPWAMP_TESTING_VARIANT
