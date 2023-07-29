/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
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

//------------------------------------------------------------------------------
template <typename T>
using VariantConstructionOp = decltype(Variant(std::declval<T>()));

//------------------------------------------------------------------------------
template <typename T, VariantKind Kind, typename U>
void checkInit(const U& initValue)
{
    INFO( "For field type '" << typeNameOf<T>() <<
          "'' and argument '" << Variant(initValue) << "'");

    T checkValue = initValue;

    {
        INFO( "Checking construction" );
        Variant v(initValue);
        CHECK( v.is<T>() );
        CHECK( v.as<T>() == checkValue );
        CHECK( v.as<Kind>() == checkValue );
        CHECK( v == checkValue );
        CHECK(( v == initValue ));
        CHECK( v == Variant(initValue) );
    }

    {
        INFO( "Checking copy construction" );
        Variant v(initValue);
        Variant w(v);
        CHECK( v.is<T>() );
        CHECK( v.as<T>() == checkValue );
        CHECK( v.as<Kind>() == checkValue );
        CHECK( v == checkValue );
        CHECK( v == Variant(initValue) );
        CHECK( w.is<T>() );
        CHECK( w.as<T>() == checkValue );
        CHECK( w.as<Kind>() == checkValue );
        CHECK( w == checkValue );
        CHECK( w == Variant(initValue) );
    }

    {
        INFO( "Checking move construction with value" );
        auto initValueCopy(initValue);
        Variant v(std::move(initValueCopy));
        CHECK( v.is<T>() );
        CHECK( v.as<T>() == checkValue );
        CHECK( v.as<Kind>() == checkValue );
        CHECK( v == checkValue );
        CHECK( v == Variant(initValue) );
    }

    {
        INFO( "Checking move construction with variant" );
        Variant v(initValue);
        Variant w(std::move(v));

        // Supress linter warnings about calling methods on moved-from object,
        // as we are actually testing the behavior of a moved-from Variant
        CHECK( v.is<Null>() ); // NOLINT
        CHECK( v.as<Null>() == null ); // NOLINT
        CHECK( v.as<VariantKind::null>() == null ); // NOLINT
        CHECK( v == null ); // NOLINT
        CHECK( v == Variant() ); // NOLINT

        CHECK( w.is<T>() );
        CHECK( w.as<T>() == checkValue );
        CHECK( w.as<Kind>() == checkValue );
        CHECK( w == checkValue );
        CHECK( w == Variant(initValue) );
    }

    {
        INFO( "Checking assignment with value" );
        Variant v;
        v = initValue;
        CHECK( v.is<T>() );
        CHECK( v.as<T>() == checkValue );
        CHECK( v.as<Kind>() == checkValue );
        CHECK( v == checkValue );
        CHECK( v == Variant(initValue) );
    }

    {
        INFO( "Checking copy assignment" );
        Variant v(initValue);
        Variant w;
        w = v;
        CHECK( v.is<T>() );
        CHECK( v.as<T>() == checkValue );
        CHECK( v.as<Kind>() == checkValue );
        CHECK( v == checkValue );
        CHECK( v == Variant(initValue) );
        CHECK( w.is<T>() );
        CHECK( w.as<T>() == checkValue );
        CHECK( w.as<Kind>() == checkValue );
        CHECK( w == checkValue );
        CHECK( w == Variant(initValue) );
    }
}

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

        // Supress linter warnings about calling methods on moved-from object,
        // as we are actually testing the behavior of a moved-from Variant
        CHECK( w.is<Null>() ); // NOLINT
        CHECK( w.as<Null>() == null ); // NOLINT
        CHECK( ((const Variant&)w).as<Null>() == null ); // NOLINT
        CHECK( w == null ); // NOLINT
        CHECK( w == Variant() ); // NOLINT
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
    checkAssign<T>(Blob{},              to);
    checkAssign<T>(Blob{0x00},          to);
    checkAssign<T>(Blob{0x00,0x01,0x02},to);
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
SCENARIO( "Variant initialization", "[Variant]" )
{
GIVEN( "a default-constructed Variant" )
{
    using T = Null;
    Variant v;
    CHECK( v.is<T>() );
    CHECK( v.as<T>() == null );
    CHECK( v == null );
    CHECK( v == Variant() );
}
GIVEN( "a Variant initialized with null" )
{
    checkInit<Null,VariantKind::null>(null);
}
GIVEN( "Variants initialized with boolean values" )
{
    using T = Bool;
    constexpr VariantKind kind = VariantKind::boolean;
    checkInit<T,kind>(false);
    checkInit<T,kind>(true);
}
GIVEN( "Variants initialized with signed integer values" )
{
    using T = Int;
    constexpr VariantKind kind = VariantKind::integer;
    checkInit<T,kind>(0);
    checkInit<T,kind>(std::numeric_limits<T>::max());
    checkInit<T,kind>(std::numeric_limits<T>::min());
    checkInit<T,kind>((signed char)-1);
    checkInit<T,kind>((short)-2);
    checkInit<T,kind>(-3);
    checkInit<T,kind>(-4l);
    checkInit<T,kind>(-5ll);
}
GIVEN( "Variants initialized with unsigned integer values" )
{
    using T = UInt;
    constexpr VariantKind kind = VariantKind::uint;
    checkInit<T,kind>(0u);
    checkInit<T,kind>(std::numeric_limits<T>::max());
    checkInit<T,kind>((unsigned char)1);
    checkInit<T,kind>((unsigned short)2);
    checkInit<T,kind>(3u);
    checkInit<T,kind>(4ul);
    checkInit<T,kind>(5ull);
}
GIVEN( "Variants initialized with floating-point values" )
{
    using T = Real;
    constexpr VariantKind kind = VariantKind::real;
    checkInit<T,kind>(0.0);
    checkInit<T,kind>(std::numeric_limits<double>::max());
    checkInit<T,kind>(std::numeric_limits<float>::min());
    checkInit<T,kind>(0.0f);
    checkInit<T,kind>(std::numeric_limits<float>::max());
    checkInit<T,kind>(std::numeric_limits<float>::min());
}
GIVEN( "Variants initialized with string values" )
{
    using T = String;
    constexpr VariantKind kind = VariantKind::string;
    checkInit<T,kind>(T("Hello"));
    checkInit<T,kind>(T(""));
    checkInit<T,kind>(T("null"));
    checkInit<T,kind>(T("true"));
    checkInit<T,kind>(T("false"));
    checkInit<T,kind>(T("0"));
    checkInit<T,kind>("Hello");

    char charArray[10] = "charArray";
    checkInit<T,kind>(charArray);

    const char constCharArray[15] = "constCharArray";
    checkInit<T,kind>(constCharArray);

    char* charPtr = charArray;
    checkInit<T,kind>(charPtr);

    const char* constCharPtr = "constCharPtr";
    checkInit<T,kind>(constCharPtr);
}
GIVEN( "Variants initialized with Blob values" )
{
    using T = Blob;
    constexpr VariantKind kind = VariantKind::blob;
    checkInit<T,kind>(T{});
    checkInit<T,kind>(T{});
    checkInit<T,kind>(T{0x00, 0x01, 0x02});
    std::vector<uint8_t> data{0x00, 0x01, 0x02};
    checkInit<T,kind>(T(data));
}
GIVEN( "Variants initialized with array values" )
{
    using T = Array;
    constexpr VariantKind kind = VariantKind::array;
    checkInit<T,kind>(T{});
    checkInit<T,kind>(T{42, "foo", true, null, 123.4});
}
GIVEN( "Variants initialized with object values" )
{
    using T = Object;
    constexpr VariantKind kind = VariantKind::object;
    checkInit<T,kind>(T{});
    checkInit<T,kind>(T{ {"a", 42}, {"b", "foo"}, {"c", true},
                         {"d", null}, {"e", 123.4} });
}

GIVEN( "Variants initialized with invalid value types" )
{
    struct Invalid {};

    CHECK(isDetected<VariantConstructionOp, int>());
    CHECK_FALSE(isDetected<VariantConstructionOp, Invalid>());
}
}

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
    WHEN( "assigning to String" )
    {
        checkCompositeAssign(String{"foo"});
        checkCompositeAssign(String{""});
        checkCompositeAssign(String{"null"});
        checkCompositeAssign(String{"true"});
        checkCompositeAssign(String{"false"});
        checkCompositeAssign(String{"0"});
        checkCompositeAssign(String{"{}"});
        checkCompositeAssign(String{"[]"});
    }
    WHEN( "assigning to Blob" )
    {
        checkCompositeAssign(Blob{});
        checkCompositeAssign(Blob{0x00});
        checkCompositeAssign(Blob{0x00, 0x01, 0x02});
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
