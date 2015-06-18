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
template <typename TTo>
void checkConvert(int level, const Variant& from, const TTo& to)
{
    INFO( "For type#" << level << ", converting from '" << from <<
          "' to '" << to << "'");
    CHECK( from.to<TTo>() == to );
    CHECK( from.valueOr(to) == to );
    CHECK( from.valueOr(TTo()) == to );
    TTo val;
    from.to(val);
    CHECK( val == to );

    Variant nullVariant;
    CHECK( nullVariant.valueOr(to) == to );
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
                CHECK_THROWS_AS( from.valueOr(val), error::Conversion );
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
    CHECK( to.typeId() == v.typeId() );
    CHECK( to == v );

    Variant from = Variant::from(v);
    CHECK( from.typeId() == v.typeId() );
    CHECK( from == v );
}

} // namespace anonymous

//------------------------------------------------------------------------------
SCENARIO( "Variant conversions", "[Variant]" )
{
GIVEN( "a Null Variant" )
{
    Variant v(null);
    WHEN ( "converted to invalid types" )
    {
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                        String, Array, Object>(v);
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
        checkBadConvert<Null, String, Array, Object>(Variant(false));
        checkBadConvert<Null, String, Array, Object>(Variant(true));
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
        checkBadConvert<Null, String, Array, Object>(Variant(0));
        checkBadConvert<Null, String, Array, Object>(Variant(1));
        checkBadConvert<Null, String, Array, Object>(Variant(-1));
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
               Approx(1.845e19).epsilon(0.001e19));
        CHECK( Variant((UInt)-1).to<double>() ==
               Approx(1.845e19).epsilon(0.001e19));
    }
    WHEN ( "converted to invalid types" )
    {
        checkBadConvert<Null, String, Array, Object>(Variant(0u));
        checkBadConvert<Null, String, Array, Object>(Variant(1u));
        checkBadConvert<Null, String, Array, Object>(Variant(-1u));
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
        checkBadConvert<Null, String, Array, Object>(Variant(0.0));
        checkBadConvert<Null, String, Array, Object>(Variant(1.0));
        checkBadConvert<Null, String, Array, Object>(Variant(-1.0));
        checkBadConvert<Null, String, Array, Object>(Variant(42.1));
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
                Array, Object, const char*>(Variant("Hello"));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                Array, Object, const char*>(Variant(""));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                Array, Object, const char*>(Variant("null"));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                Array, Object, const char*>(Variant("false"));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                Array, Object, const char*>(Variant("true"));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                Array, Object, const char*>(Variant("0"));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                Array, Object, const char*>(Variant("1"));
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
                String, Object>(Variant(Array{}));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                String, Object>(Variant(Array{null}));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                String, Object>(Variant(Array{false}));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                String, Object>(Variant(Array{true}));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                String, Object>(Variant(Array{0u}));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                String, Object>(Variant(Array{-1}));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                String, Object>(Variant(Array{0.0}));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                String, Object>(Variant(Array{""}));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                String, Object>(Variant(Array{Array{}}));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                String, Object>(Variant(Array{Object{}}));
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
                String, Array>(Variant(Object{}));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                String, Array>(Variant(Object{ {"null",null} }));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                String, Array>(Variant(Object{ {"false",false} }));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                String, Array>(Variant(Object{ {"true",true} }));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                String, Array>(Variant(Object{ {"0",0u} }));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                String, Array>(Variant(Object{ {"-1",-1} }));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                String, Array>(Variant(Object{ {"0.0",0.0} }));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                String, Array>(Variant(Object{ {"",""} }));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                String, Array>(Variant(Object{ {"[]",Array{}} }));
        checkBadConvert<Bool,signed char, unsigned short, int, Int, UInt, Real,
                String, Array>(Variant(Object{ {"{}",Object{}} }));
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
        checkVariantToVariantConvert(Array{null, true, 42, 123u, 3.14, "hello"});
        checkVariantToVariantConvert(Object{ {{"a"},{1}}, {{"b"},{"foo"}} });
    }
}
}

#endif // #if CPPWAMP_TESTING_VARIANT
