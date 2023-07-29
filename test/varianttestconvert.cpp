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
template <typename TTo>
void checkConvert(int level, const Variant& from, const TTo& to)
{
    INFO( "For type#" << level << ", converting from '" << from <<
          "' to '" << to << "'");
    CHECK( from.to<TTo>() == to );
    CHECK( from.valueOr<TTo>(to) == to );
    CHECK( from.valueOr<TTo>(TTo()) == to );
    TTo val;
    from.to(val);
    CHECK( val == to );

    Variant nullVariant;
    CHECK( nullVariant.valueOr<TTo>(to) == to );
}

//------------------------------------------------------------------------------
template <typename TTo, typename... TTos>
void checkConvert(int level, const Variant& from, const TTo& to, TTos... tos)
{
    {
        INFO( "For type#" << level << ", converting from '" << from <<
              "' to '" << to << "'");
        CHECK( from.to<TTo>() == to );
    }
    checkConvert(level+1, from, tos...);
}

//------------------------------------------------------------------------------
template <typename... TTos>
void checkConvert(const Variant& from, TTos... tos)
{
    checkConvert(0, from, tos...);
}

//------------------------------------------------------------------------------
template <typename... TTo> struct CheckBadConvert
{
    static void check(const Variant&, int) {}
};

//------------------------------------------------------------------------------
template <typename TTo, typename... TTos> struct CheckBadConvert<TTo, TTos...>
{
    static void check(const Variant& from, int level = 0)
    {
        {
            INFO( "For type #" << level );
            CHECK_THROWS_AS( from.to<TTo>(), error::Conversion );
            TTo val;
            CHECK_THROWS_AS( from.to(val), error::Conversion );
            if (!!from)
                CHECK_THROWS_AS( from.valueOr<TTo>(val), error::Conversion );
        }
        CheckBadConvert<TTos...>::check(from, level+1);
    }
};

//------------------------------------------------------------------------------
template <typename... TTos>
void checkBadConvert(const Variant& from)
{
    CheckBadConvert<TTos...>::check(from);
}

//------------------------------------------------------------------------------
void checkVariantToVariantConvert(Variant v)
{
    INFO( "For Variant = " << v );

    auto to = v.to<Variant>();
    CHECK( to.kind() == v.kind() );
    CHECK( to == v );

    Variant from = Variant::from(v);
    CHECK( from.kind() == v.kind() );
    CHECK( from == v );
}

//------------------------------------------------------------------------------
template <typename T>
void checkBadAccess(const T& value)
{
    using K = VariantKind;
    Variant v(value);
    const Variant& cv = v;
    INFO( "For variant of type '" << typeNameOf(v) <<
          "' and value '" << v << "'");
    if (!v.is<Null>())
    {
        CHECK_THROWS_AS( v.as<Null>(), error::Access );
        CHECK_THROWS_AS( cv.as<Null>(), error::Access );
        CHECK_THROWS_AS( v.as<K::null>(), error::Access );
        CHECK_THROWS_AS( cv.as<K::null>(), error::Access );
    }
    if (!v.is<Bool>())
    {
        CHECK_THROWS_AS( v.as<Bool>(), error::Access );
        CHECK_THROWS_AS( cv.as<Bool>(), error::Access );
        CHECK_THROWS_AS( v.as<K::boolean>(), error::Access );
        CHECK_THROWS_AS( cv.as<K::boolean>(), error::Access );
    }
    if (!v.is<Int>())
    {
        CHECK_THROWS_AS( v.as<Int>(), error::Access );
        CHECK_THROWS_AS( cv.as<Int>(), error::Access );
        CHECK_THROWS_AS( v.as<K::integer>(), error::Access );
        CHECK_THROWS_AS( cv.as<K::integer>(), error::Access );
    }
    if (!v.is<UInt>())
    {
        CHECK_THROWS_AS( v.as<UInt>(), error::Access );
        CHECK_THROWS_AS( cv.as<UInt>(), error::Access );
        CHECK_THROWS_AS( v.as<K::uint>(), error::Access );
        CHECK_THROWS_AS( cv.as<K::uint>(), error::Access );
    }
    if (!v.is<Real>())
    {
        CHECK_THROWS_AS( v.as<Real>(), error::Access );
        CHECK_THROWS_AS( cv.as<Real>(), error::Access );
        CHECK_THROWS_AS( v.as<K::real>(), error::Access );
        CHECK_THROWS_AS( cv.as<K::real>(), error::Access );
    }
    if (!v.is<String>())
    {
        CHECK_THROWS_AS( v.as<String>(), error::Access );
        CHECK_THROWS_AS( cv.as<String>(), error::Access );
        CHECK_THROWS_AS( v.as<K::string>(), error::Access );
        CHECK_THROWS_AS( cv.as<K::string>(), error::Access );
    }
    if (!v.is<Blob>())
    {
        CHECK_THROWS_AS( v.as<Blob>(), error::Access );
        CHECK_THROWS_AS( cv.as<Blob>(), error::Access );
        CHECK_THROWS_AS( v.as<K::blob>(), error::Access );
        CHECK_THROWS_AS( cv.as<K::blob>(), error::Access );
    }
    if (!v.is<Array>())
    {
        CHECK_THROWS_AS( v.as<Array>(), error::Access );
        CHECK_THROWS_AS( cv.as<Array>(), error::Access );
        CHECK_THROWS_AS( v.as<K::array>(), error::Access );
        CHECK_THROWS_AS( cv.as<K::array>(), error::Access );
    }
    if (!v.is<Object>())
    {
        CHECK_THROWS_AS( v.as<Object>(), error::Access );
        CHECK_THROWS_AS( cv.as<Object>(), error::Access );
        CHECK_THROWS_AS( v.as<K::object>(), error::Access );
        CHECK_THROWS_AS( cv.as<K::object>(), error::Access );
    }
}

} // namespace anonymous

namespace user
{

enum class UserEnum
{
    foo,
    bar
};

enum class StrEnum
{
    foo,
    bar
};

void convert(FromVariantConverter& c, StrEnum& e)
{
    const auto& s = c.variant().as<String>();
    if (s == "foo")
        e = StrEnum::foo;
    else if (s == "bar")
        e = StrEnum::bar;
    else
        throw error::Conversion("invalid enumeration string");
}

void convert(ToVariantConverter& c, StrEnum e)
{
    switch (e)
    {
    case StrEnum::foo: c(String("foo")); break;
    case StrEnum::bar: c(String("bar")); break;
    }
}

} // namespace user

//------------------------------------------------------------------------------
SCENARIO( "Variant conversions", "[Variant]" )
{
GIVEN( "a Null Variant" )
{
    Variant v(null);
    WHEN ( "converted to invalid types" )
    {
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                        String, Blob, Array, Object>(v);
    }
}
GIVEN( "Bool Variants" )
{
    WHEN( "converted to valid types" )
    {
        checkConvert(Variant(false), false, (signed char)0,
                     (unsigned short)0, 0, 0ul, 0ll, 0.0f, 0.0);
        checkConvert(Variant(true), true, (signed char)1,
                     (unsigned short)1, 1, 1ul, 1ll, 1.0f, 1.0);
    }
    WHEN ( "converted to invalid types" )
    {
        checkBadConvert<Null, String, Blob, Array, Object>(Variant(false));
        checkBadConvert<Null, String, Blob, Array, Object>(Variant(true));
    }
}
GIVEN( "Int Variants" )
{
    WHEN( "converted to valid types" )
    {
        checkConvert(Variant(0), false, (signed char)0,
                     (unsigned short)0, 0, 0ul, 0ll, 0.0f, 0.0);
        checkConvert(Variant(1), true, (unsigned char)1,
                     (signed short)1, 1u, 1l, 1ull, 1.0f, 1.0);
        checkConvert(Variant(-1), true, (unsigned char)-1,
                     (signed short)-1, -1u, -1l, -1ull, -1.0f, -1.0);
    }
    WHEN ( "converted to invalid types" )
    {
        checkBadConvert<Null, String, Blob, Array, Object>(Variant(0));
        checkBadConvert<Null, String, Blob, Array, Object>(Variant(1));
        checkBadConvert<Null, String, Blob, Array, Object>(Variant(-1));
    }
}
GIVEN( "UInt Variants" )
{
    WHEN( "converted to valid types" )
    {
        checkConvert(Variant(0u), false, (signed char)0,
                     (unsigned short)0, 0, 0ul, 0ll, 0.0f, 0.0);
        checkConvert(Variant(1u), true, (unsigned char)1,
                     (signed short)1, 1u, 1l, 1ull, 1.0f, 1.0);
        checkConvert(Variant((UInt)-1), true, (unsigned char)-1,
                     (signed short)-1, -1u, -1l, -1ull);
        CHECK( Variant((UInt)-1).to<float>() ==
               Approx(1.845e19).epsilon(0.001));
        CHECK( Variant((UInt)-1).to<double>() ==
               Approx(1.845e19).epsilon(0.001));
    }
    WHEN ( "converted to invalid types" )
    {
        checkBadConvert<Null, String, Blob, Array, Object>(Variant(0u));
        checkBadConvert<Null, String, Blob, Array, Object>(Variant(1u));
        checkBadConvert<Null, String, Blob, Array, Object>(Variant(-1u));
    }
}
GIVEN( "Real Variants" )
{
    WHEN( "converted to valid types" )
    {
        checkConvert(Variant(0.0), false, (signed char)0, (unsigned short)0, 0,
                     0ul, 0ll, 0.0f, 0.0);
        checkConvert(Variant(1.0), true, (unsigned char)1, (signed short)1, 1u,
                     1l, 1ull, 1.0f, 1.0);
        checkConvert(Variant(-1.0), true, (unsigned char)-1, (signed short)-1,
                     -1u, -1l, -1ull, -1.0f, -1.0);
        checkConvert(Variant(42.1), true, (signed char)42, (unsigned short)42,
                     42, 42ul, 42ll, 42.1);
        CHECK( Variant(42.1).to<float>() == Approx(42.1) );
    }
    WHEN ( "converted to invalid types" )
    {
        checkBadConvert<Null, String, Blob, Array, Object>(Variant(0.0));
        checkBadConvert<Null, String, Blob, Array, Object>(Variant(1.0));
        checkBadConvert<Null, String, Blob, Array, Object>(Variant(-1.0));
        checkBadConvert<Null, String, Blob, Array, Object>(Variant(42.1));
    }
}
GIVEN( "String Variants" )
{
    WHEN( "converted to valid types" )
    {
        checkConvert(Variant("Hello"),  String("Hello"));
        checkConvert(Variant(""),       String(""));
        checkConvert(Variant("null"),   String("null"));
        checkConvert(Variant("false"),  String("false"));
        checkConvert(Variant("true"),   String("true"));
        checkConvert(Variant("0"),      String("0"));
        checkConvert(Variant("1"),      String("1"));
    }
    WHEN ( "converted to invalid types" )
    {
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                Blob, Array, Object, const char*>(Variant("Hello"));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                Blob, Array, Object, const char*>(Variant(""));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                Blob, Array, Object, const char*>(Variant("null"));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                Blob, Array, Object, const char*>(Variant("false"));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                Blob, Array, Object, const char*>(Variant("true"));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                Blob, Array, Object, const char*>(Variant("0"));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                Blob, Array, Object, const char*>(Variant("1"));
    }
}
GIVEN( "Blob Variants" )
{
    WHEN( "converted to valid types" )
    {
        checkConvert(Variant(Blob{}),     Blob({}));
        checkConvert(Variant(Blob{0x00}), Blob({0x00}));
        checkConvert(Variant(Blob{0x42}), Blob({0x42}));
        checkConvert(Variant(Blob{0x01, 0x02, 0x03}), Blob({0x01, 0x02, 0x03}));
    }
    WHEN ( "converted to invalid types" )
    {
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                String, Array, Object, const char*>(Variant(Blob{}));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                String, Array, Object, const char*>(Variant(Blob{0x00}));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                String, Array, Object, const char*>(Variant(Blob{0x01}));
    }
}
GIVEN( "Array Variants" )
{
    WHEN( "converted to valid types" )
    {
        checkConvert(Variant(Array{}),         Array{});
        checkConvert(Variant(Array{null}),     Array{null});
        checkConvert(Variant(Array{false}),    Array{false});
        checkConvert(Variant(Array{true}),     Array{true});
        checkConvert(Variant(Array{0u}),       Array{0u}, Array{0}, Array{0.0});
        checkConvert(Variant(Array{-1}),       Array{-1}, Array{-1.0});
        checkConvert(Variant(Array{0.0}),      Array{0.0}, Array{0u}, Array{0});
        checkConvert(Variant(Array{""}),       Array{""});
        checkConvert(Variant(Array{Array{}}),  Array{Array{}});
        checkConvert(Variant(Array{Object{}}), Array{Object{}});
        checkConvert(Variant(Array{null,false,true,42u,-42,"hello",Array{},Object{}}),
                     Array{null,false,true,42u,-42,"hello",Array{},Object{}});
        checkConvert(Variant(Array{ Array{Array{"foo",42} }, Array{ Object{{"foo",42}} } }),
                     Array{ Array{ Array{"foo",42} }, Array{ Object{{"foo",42}} } });
    }
    WHEN ( "converted to invalid types" )
    {
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                Blob, String, Object>(Variant(Array{}));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                Blob, String, Object>(Variant(Array{null}));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                Blob, String, Object>(Variant(Array{false}));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                Blob, String, Object>(Variant(Array{true}));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                Blob, String, Object>(Variant(Array{0u}));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                Blob, String, Object>(Variant(Array{-1}));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                Blob, String, Object>(Variant(Array{0.0}));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                Blob, String, Object>(Variant(Array{""}));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                Blob, String, Object>(Variant(Array{Array{}}));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                Blob, String, Object>(Variant(Array{Object{}}));
    }
}
GIVEN( "Object Variants" )
{
    WHEN( "converted to valid types" )
    {
        checkConvert(Variant(Object{}), Object{});
        checkConvert(Variant(Object{ {"null",null} }), Object{ {"null",null} });
        checkConvert(Variant(Object{ {"false",false} }), Object{ {"false",false} });
        checkConvert(Variant(Object{ {"true",true} }), Object{ {"true",true} });
        checkConvert(Variant(Object{ {"0",0u} }), Object{{"0",0u}}, Object{{"0",0}}, Object{{"0",0.0}} );
        checkConvert(Variant(Object{ {"-1",-1} }), Object{{"-1",-1}}, Object{{"-1",-1.0}});
        checkConvert(Variant(Object{ {"0.0",0.0} }), Object{{"0.0",0.0}}, Object{{"0.0",0}}, Object{{"0.0",0u}});
        checkConvert(Variant(Object{ {"",""} }), Object{ {"",""} });
        checkConvert(Variant(Object{ {"[]",Array{}} }), Object{ {"[]",Array{}} });
        checkConvert(Variant(Object{ {"{}",Object{}} }), Object{ {"{}",Object{}} });

        checkConvert(Variant(
            Object{ {"null",null}, {"false",false}, {"true",true}, {"0",0u},
                    {"-1",-1}, {"0.0",0.0}, {"",""}, {"[]",Array{}}, {"{}",Object{}} }),
            Object{ {"null",null}, {"false",false}, {"true",true}, {"0",0u},
                    {"-1",-1}, {"0.0",0.0}, {"",""}, {"[]",Array{}}, {"{}",Object{}} });

        checkConvert(Variant(
            Object{ {"foo",
                    Object{ {"bar",
                            Object{ {"baz", 42} } } } } } ),
            Object{ {"foo",
                    Object{ {"bar",
                            Object{ {"baz", 42} } } } } } );
    }
    WHEN ( "converted to invalid types" )
    {
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                String, Blob, Array>(Variant(Object{}));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                String, Blob, Array>(Variant(Object{ {"null",null} }));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                String, Blob, Array>(Variant(Object{ {"false",false} }));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                String, Blob, Array>(Variant(Object{ {"true",true} }));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                String, Blob, Array>(Variant(Object{ {"0",0u} }));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                String, Blob, Array>(Variant(Object{ {"-1",-1} }));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                String, Blob, Array>(Variant(Object{ {"0.0",0.0} }));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                String, Blob, Array>(Variant(Object{ {"",""} }));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                String, Blob, Array>(Variant(Object{ {"[]",Array{}} }));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                String, Blob, Array>(Variant(Object{ {"{}",Object{}} }));
    }
}
}

//------------------------------------------------------------------------------
SCENARIO( "Variant-to-Variant Conversions", "[Variant]" )
{
GIVEN( "an assortment of variants" )
{
    WHEN ( "converting to a variant" )
    {
        checkVariantToVariantConvert(null);
        checkVariantToVariantConvert(false);
        checkVariantToVariantConvert(true);
        checkVariantToVariantConvert(42);
        checkVariantToVariantConvert(123u);
        checkVariantToVariantConvert(3.14);
        checkVariantToVariantConvert("hello");
        checkVariantToVariantConvert(Blob{0x42});
        checkVariantToVariantConvert(Array{null, true, 42, 123u, 3.14, "hello"});
        checkVariantToVariantConvert(Object{ {{"a"},{1}}, {{"b"},{"foo"}} });
    }
}
}

//------------------------------------------------------------------------------
SCENARIO( "Variant-enum conversions", "[Variant]" )
{
GIVEN( "an enumerator without a custom converter" )
{
    user::UserEnum e = {};

    WHEN ( "converting to variant" )
    {
        e = user::UserEnum::bar;
        auto v = Variant::from(e);

        THEN( "the variant contains the integral value" )
        {
            REQUIRE( v.is<Int>() );
            auto n = v.as<Int>();
            using U = std::underlying_type<user::UserEnum>::type;
            CHECK( n == static_cast<U>(user::UserEnum::bar) );
        }
    }

    WHEN ( "converting from variant" )
    {
        auto v = Variant::from(user::UserEnum::bar);
        e = v.to<user::UserEnum>();
        CHECK( e == user::UserEnum::bar );
    }

    WHEN ( "converting from invalid variant" )
    {
        Variant v = "bar";
        CHECK_THROWS_AS( v.to<user::UserEnum>(), error::Conversion );
    }
}

GIVEN( "an enumerator with a custom converter" )
{
    user::StrEnum e = {};

    WHEN ( "converting to variant" )
    {
        e = user::StrEnum::bar;
        auto v = Variant::from(e);

        THEN( "the variant contains a string value" )
        {
            REQUIRE( v.is<String>() );
            auto s = v.as<String>();
            CHECK( s == "bar" );
        }
    }

    WHEN ( "converting from variant" )
    {
        Variant v = "bar";
        auto e = v.to<user::StrEnum>();
        CHECK( e == user::StrEnum::bar );
    }

    WHEN ( "converting from invalid variant" )
    {
        Variant v = 1;
        CHECK_THROWS_AS( v.to<user::StrEnum>(), error::Access );
    }
}
}

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
