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
template <typename T, TypeId typeId, typename U>
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
        CHECK( v.as<typeId>() == checkValue );
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
        CHECK( v.as<typeId>() == checkValue );
        CHECK( v == checkValue );
        CHECK( v == Variant(initValue) );
        CHECK( w.is<T>() );
        CHECK( w.as<T>() == checkValue );
        CHECK( w.as<typeId>() == checkValue );
        CHECK( w == checkValue );
        CHECK( w == Variant(initValue) );
    }

    {
        INFO( "Checking move construction with value" );
        auto initValueCopy(initValue);
        Variant v(std::move(initValueCopy));
        CHECK( v.is<T>() );
        CHECK( v.as<T>() == checkValue );
        CHECK( v.as<typeId>() == checkValue );
        CHECK( v == checkValue );
        CHECK( v == Variant(initValue) );
    }

    {
        INFO( "Checking move construction with variant" );
        Variant v(initValue);
        Variant w(std::move(v));
        CHECK( v.is<Null>() );
        CHECK( v.as<Null>() == null );
        CHECK( v.as<TypeId::null>() == null );
        CHECK( v == null );
        CHECK( v == Variant() );
        CHECK( w.is<T>() );
        CHECK( w.as<T>() == checkValue );
        CHECK( w.as<typeId>() == checkValue );
        CHECK( w == checkValue );
        CHECK( w == Variant(initValue) );
    }

    {
        INFO( "Checking assignment with value" );
        Variant v;
        v = initValue;
        CHECK( v.is<T>() );
        CHECK( v.as<T>() == checkValue );
        CHECK( v.as<typeId>() == checkValue );
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
        CHECK( v.as<typeId>() == checkValue );
        CHECK( v == checkValue );
        CHECK( v == Variant(initValue) );
        CHECK( w.is<T>() );
        CHECK( w.as<T>() == checkValue );
        CHECK( w.as<typeId>() == checkValue );
        CHECK( w == checkValue );
        CHECK( w == Variant(initValue) );
    }
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
    checkInit<Null,TypeId::null>(null);
}
GIVEN( "Variants initialized with boolean values" )
{
    using T = Bool;
    constexpr TypeId id = TypeId::boolean;
    checkInit<T,id>(false);
    checkInit<T,id>(true);
}
GIVEN( "Variants initialized with signed integer values" )
{
    using T = Int;
    constexpr TypeId id = TypeId::integer;
    checkInit<T,id>(0);
    checkInit<T,id>(std::numeric_limits<T>::max());
    checkInit<T,id>(std::numeric_limits<T>::min());
    checkInit<T,id>((signed char)-1);
    checkInit<T,id>((short)-2);
    checkInit<T,id>(-3);
    checkInit<T,id>(-4l);
    checkInit<T,id>(-5ll);
}
GIVEN( "Variants initialized with unsigned integer values" )
{
    using T = UInt;
    constexpr TypeId id = TypeId::uint;
    checkInit<T,id>(0u);
    checkInit<T,id>(std::numeric_limits<T>::max());
    checkInit<T,id>((unsigned char)1);
    checkInit<T,id>((unsigned short)2);
    checkInit<T,id>(3u);
    checkInit<T,id>(4ul);
    checkInit<T,id>(5ull);
}
GIVEN( "Variants initialized with floating-point values" )
{
    using T = Real;
    constexpr TypeId id = TypeId::real;
    checkInit<T,id>(0.0);
    checkInit<T,id>(std::numeric_limits<double>::max());
    checkInit<T,id>(std::numeric_limits<float>::min());
    checkInit<T,id>(0.0f);
    checkInit<T,id>(std::numeric_limits<float>::max());
    checkInit<T,id>(std::numeric_limits<float>::min());
}
GIVEN( "Variants initialized with string values" )
{
    using T = String;
    constexpr TypeId id = TypeId::string;
    checkInit<T,id>(T("Hello"));
    checkInit<T,id>(T(""));
    checkInit<T,id>(T("null"));
    checkInit<T,id>(T("true"));
    checkInit<T,id>(T("false"));
    checkInit<T,id>(T("0"));
    checkInit<T,id>("Hello");

    char charArray[10] = "charArray";
    checkInit<T,id>(charArray);

    const char constCharArray[15] = "constCharArray";
    checkInit<T,id>(constCharArray);

    char* charPtr = charArray;
    checkInit<T,id>(charPtr);

    const char* constCharPtr = "constCharPtr";
    checkInit<T,id>(constCharPtr);
}
GIVEN( "Variants initialized with array values" )
{
    using T = Array;
    constexpr TypeId id = TypeId::array;
    checkInit<T,id>(T{});
    checkInit<T,id>(T{42, "foo", true, null, 123.4});
}
GIVEN( "Variants initialized with object values" )
{
    using T = Object;
    constexpr TypeId id = TypeId::object;
    checkInit<T,id>(T{});
    checkInit<T,id>(T{ {"a", 42}, {"b", "foo"}, {"c", true},
                       {"d", null}, {"e", 123.4} });
}

// The following should generate a "no matching function" compiler error.
//GIVEN( "Variants initialized with invalid value types" )
//{
//    struct Invalid {};
//    Invalid invalid;
//    Variant v(invalid); // Should generate compiler error
//}
}

#endif // #if CPPWAMP_TESTING_VARIANT
