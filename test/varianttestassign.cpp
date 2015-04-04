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

//------------------------------------------------------------------------------
template <typename TExpected, typename TFrom, typename TTo>
void checkAssign(const TFrom& from, const TTo& to)
{
    INFO( "For field type '" << typeNameOf<TExpected>() <<
          "' from '" << Variant(from) << "' to '" << Variant(to) << "'" );

    TExpected checkValue = to;

    {
        INFO( "Checking assignment to value" );
        Variant v(from);
        v = to;
        CHECK( v.is<TExpected>() );
        CHECK( v.as<TExpected>() == checkValue );
        CHECK( ((const Variant&)v).as<TExpected>() == checkValue );
        CHECK( v == checkValue );
        CHECK( v == Variant(checkValue) );
    }

    {
        INFO( "Checking move assignment to value" );
        auto toCopy(to);
        Variant v(from);
        v = std::move(toCopy);
        CHECK( v.is<TExpected>() );
        CHECK( v.as<TExpected>() == checkValue );
        CHECK( ((const Variant&)v).as<TExpected>() == checkValue );
        CHECK( v == checkValue );
        CHECK( v == Variant(checkValue) );
    }

    {
        INFO( "Checking assignment to Variant" );
        Variant v(from);
        Variant w(to);
        v = w;
        CHECK( v.is<TExpected>() );
        CHECK( v.as<TExpected>() == checkValue );
        CHECK( ((const Variant&)v).as<TExpected>() == checkValue );
        CHECK( v == checkValue );
        CHECK( v == Variant(checkValue) );
        CHECK( w.is<TExpected>() );
        CHECK( w.as<TExpected>() == checkValue );
        CHECK( ((const Variant&)w).as<TExpected>() == checkValue );
        CHECK( w == checkValue );
        CHECK( w == Variant(checkValue) );
    }

    {
        INFO( "Checking move assignment to Variant" );
        Variant v(from);
        Variant w(to);
        v = std::move(w);
        CHECK( v.is<TExpected>() );
        CHECK( v.as<TExpected>() == checkValue );
        CHECK( ((const Variant&)v).as<TExpected>() == checkValue );
        CHECK( v == checkValue );
        CHECK( v == Variant(checkValue) );
        CHECK( w.is<Null>() );
        CHECK( w.as<Null>() == null );
        CHECK( ((const Variant&)w).as<Null>() == null );
        CHECK( w == null );
        CHECK( w == Variant() );
    }
}

//------------------------------------------------------------------------------
template <typename TExpected, typename TTo, typename TFrom>
void checkScalarAssign(const TFrom& from)
{
    if (std::is_signed<TTo>::value)
        checkAssign<TExpected,TFrom,TTo>(from, std::numeric_limits<TTo>::min());
    checkAssign<TExpected,TFrom,TTo>(from, TTo(0));
    checkAssign<TExpected,TFrom,TTo>(from, std::numeric_limits<TTo>::max());
}

//------------------------------------------------------------------------------
template <typename TExpected, typename TTo, typename TFrom>
void checkScalarAssign()
{
    if (std::is_signed<TFrom>::value)
        checkScalarAssign<TExpected,TTo,TFrom>(std::numeric_limits<TFrom>::min());
    checkScalarAssign<TExpected,TTo,TFrom>(TFrom(0));
    checkScalarAssign<TExpected,TTo,TFrom>(std::numeric_limits<TFrom>::max());
}

//------------------------------------------------------------------------------
template <typename TExpected, typename TTo>
void checkNumberAssign()
{
    checkScalarAssign<TExpected,TTo,Bool>();
    checkScalarAssign<TExpected,TTo,Int>();
    checkScalarAssign<TExpected,TTo,UInt>();
    checkScalarAssign<TExpected,TTo,Real>();
    checkScalarAssign<TExpected,TTo>("");
    checkScalarAssign<TExpected,TTo>("0");
    checkScalarAssign<TExpected,TTo>("null");
    checkScalarAssign<TExpected,TTo>(Array{});
    checkScalarAssign<TExpected,TTo>(Array{0});
    checkScalarAssign<TExpected,TTo>(Object{});
    checkScalarAssign<TExpected,TTo>(Object{{"0",0}});
}

//------------------------------------------------------------------------------
template <typename TComposite>
void checkCompositeAssign(const TComposite& to)
{
    auto intMin  = std::numeric_limits<Int>::min();
    auto intMax  = std::numeric_limits<Int>::max();
    auto uintMax = std::numeric_limits<UInt>::max();
    auto realMin = std::numeric_limits<Real>::lowest();
    auto realMax = std::numeric_limits<Real>::max();

    using T = TComposite;
    checkAssign<T>(false,    to);
    checkAssign<T>(true,     to);
    checkAssign<T>(intMin,   to);
    checkAssign<T>(0,        to);
    checkAssign<T>(intMax,   to);
    checkAssign<T>(0,        to);
    checkAssign<T>(uintMax,  to);
    checkAssign<T>(realMin,  to);
    checkAssign<T>(0,        to);
    checkAssign<T>(realMax,  to);
    checkAssign<T>("",       to);
    checkAssign<T>("{}",     to);
    checkAssign<T>("[]",     to);
    checkAssign<T>("{0}",    to);
    checkAssign<T>("[0]",    to);
    checkAssign<T>("0",      to);
    checkAssign<T>(Array{},             to);
    checkAssign<T>(Array{0},            to);
    checkAssign<T>(Array{""},           to);
    checkAssign<T>(Array{"0"},          to);
    checkAssign<T>(Array{"{}"},         to);
    checkAssign<T>(Array{"[]"},         to);
    checkAssign<T>(Array{"foo", 42},    to);
    checkAssign<T>(Object{},            to);
    checkAssign<T>(Object{{"",""}},     to);
    checkAssign<T>(Object{{"0",0}},     to);
    checkAssign<T>(Object{{"[]","{}"}}, to);
    checkAssign<T>(Object{{"foo",42}},  to);
}

} // namespace anonymous

//------------------------------------------------------------------------------
SCENARIO( "Assigning variants", "[Variant]" )
{
GIVEN( "assorted variants" )
{
    auto intMin  = std::numeric_limits<Int>::min();
    auto intMax  = std::numeric_limits<Int>::max();
    auto uintMax = std::numeric_limits<UInt>::max();
    auto realMin = std::numeric_limits<Real>::lowest();
    auto realMax = std::numeric_limits<Real>::max();

    WHEN( "assigning to Null" )
    {
        using T = Null;
        checkAssign<T>(false,    null);
        checkAssign<T>(true,     null);
        checkAssign<T>(intMin,   null);
        checkAssign<T>(0,        null);
        checkAssign<T>(intMax,   null);
        checkAssign<T>(0,        null);
        checkAssign<T>(uintMax,  null);
        checkAssign<T>(realMin,  null);
        checkAssign<T>(0,        null);
        checkAssign<T>(realMax,  null);
        checkAssign<T>("",       null);
        checkAssign<T>("null",   null);
        checkAssign<T>(Array{},  null);
        checkAssign<T>(Array{0}, null);
        checkAssign<T>(Object{}, null);
        checkAssign<T>(Object{{"null",0}}, null);
    }
    WHEN( "assigning to Bool" )
    {
        using T = Bool;
        checkNumberAssign<Bool,Bool>();
        checkScalarAssign<T,T>("false");
        checkScalarAssign<T,T>("true");
        checkScalarAssign<T,T>("0");
        checkScalarAssign<T,T>("1");
        checkScalarAssign<T,T>(Array{false});
        checkScalarAssign<T,T>(Array{true});
        checkScalarAssign<T,T>(Object{{"false",false}});
        checkScalarAssign<T,T>(Object{{"true",true}});
    }
    WHEN( "assigning to Int" )
    {
        checkNumberAssign<Int,signed char>();
        checkNumberAssign<Int,short>();
        checkNumberAssign<Int,int>();
        checkNumberAssign<Int,Int>();
    }
    WHEN( "assigning to UInt" )
    {
        checkNumberAssign<UInt,unsigned char>();
        checkNumberAssign<UInt,unsigned short>();
        checkNumberAssign<UInt,unsigned int>();
        checkNumberAssign<UInt,UInt>();
    }
    WHEN( "assigning to Real" )
    {
        checkNumberAssign<Real,float>();
        checkNumberAssign<Real,Real>();
    }
    WHEN( "assigning to Array" )
    {
        checkCompositeAssign(Array{});
        checkCompositeAssign(Array{0});
        checkCompositeAssign(Array{""});
        checkCompositeAssign(Array{"0"});
        checkCompositeAssign(Array{"{}"});
        checkCompositeAssign(Array{"[]"});
        checkCompositeAssign(Array{"foo", 42});
    }
    WHEN( "assigning to Object" )
    {
        checkCompositeAssign(Object{});
        checkCompositeAssign(Object{{"",""}});
        checkCompositeAssign(Object{{"0",0}});
        checkCompositeAssign(Object{{"[]","{}"}});
        checkCompositeAssign(Object{{"foo",42}});
    }
}
}

#endif // #if CPPWAMP_TESTING_VARIANT
