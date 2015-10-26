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
template <typename TExpected>
class TestVisitor : public Visitor<bool>
{
public:
    TestVisitor(TExpected& field) : fieldPtr_(&field) {}

    bool operator()(TExpected& field) const {return &field == fieldPtr_;}

    template <typename T>
    bool operator()(T&) const {return false;}

private:
    TExpected* fieldPtr_;
};

//------------------------------------------------------------------------------
template <typename TLeft, typename TRight>
class BinaryTestVisitor : public Visitor<bool>
{
public:
    BinaryTestVisitor(const TLeft& leftField, const TRight& rightField)
        : leftFieldPtr_(&leftField), rightFieldPtr_(&rightField) {}

    bool operator()(const TLeft& leftField, const TRight& rightField) const
        {return (&leftField == leftFieldPtr_) &&
                (&rightField == rightFieldPtr_);}

    template <typename T, typename U>
    bool operator()(const T&, const U&) const {return false;}

private:
    const TLeft* leftFieldPtr_;
    const TRight* rightFieldPtr_;
};

//------------------------------------------------------------------------------
template <typename T>
void checkVisitation(const T& value)
{
    Variant v(value);
    INFO( "For type '" << typeNameOf(v) << "' and value '" << v << "'" );
    CHECK( apply(TestVisitor<T>(v.as<T>()), v) );
}

//------------------------------------------------------------------------------
template <typename T, typename U>
void checkBinaryVisitation(const T& left, const U& right)
{
    Variant v(left);
    Variant w(right);
    INFO( "For types (" << typeNameOf(v) << "," << typeNameOf(w) <<
          ") and values (" << v << "," << w << ")" );
    BinaryTestVisitor<T,U> visitor(v.as<T>(), w.as<U>());
    CHECK( apply(visitor, v, w) );

    BinaryTestVisitor<T,U> rhsValueVisitor(v.as<T>(), right);
    CHECK( applyWithOperand(rhsValueVisitor, v, right) );
}

//------------------------------------------------------------------------------
template <typename T>
void checkBinaryVisitation(const T& left)
{
    Variant v(left);
    INFO( "For type '" << typeNameOf(v) << "' and value '" << v << "'" );
    checkBinaryVisitation(left, null);
    checkBinaryVisitation(left, false);
    checkBinaryVisitation(left, true);
    checkBinaryVisitation(left, Int(0));
    checkBinaryVisitation(left, UInt(0u));
    checkBinaryVisitation(left, Real(0.0));
    checkBinaryVisitation(left, String(""));
    checkBinaryVisitation(left, Blob{});
    checkBinaryVisitation(left, Array{});
    checkBinaryVisitation(left, Object{});
}

} // anonymous namespace

//------------------------------------------------------------------------------
SCENARIO( "Variant visitation", "[Variant]" )
{
auto intMin  = std::numeric_limits<Int>::min();
auto intMax  = std::numeric_limits<Int>::max();
auto uintMax = std::numeric_limits<UInt>::max();
auto realMin = std::numeric_limits<Real>::lowest();
auto realMax = std::numeric_limits<Real>::max();

GIVEN( "assorted variants" )
{
    checkVisitation(null);
    checkVisitation(false);
    checkVisitation(true);
    checkVisitation(Int(0));
    checkVisitation(intMin);
    checkVisitation(intMax);
    checkVisitation(UInt(0u));
    checkVisitation(uintMax);
    checkVisitation(Real(0.0));
    checkVisitation(realMin);
    checkVisitation(realMax);
    checkVisitation(String(""));
    checkVisitation(String("null"));
    checkVisitation(String("true"));
    checkVisitation(String("false"));
    checkVisitation(String("0"));
    checkVisitation(String("1"));
    checkVisitation(Blob{});
    checkVisitation(Blob{0x00});
    checkVisitation(Blob{0x00, 0x01, 0x02});
    checkVisitation(Array{});
    checkVisitation(Array{null});
    checkVisitation(Array{true});
    checkVisitation(Array{false});
    checkVisitation(Array{0});
    checkVisitation(Array{0u});
    checkVisitation(Array{0.0});
    checkVisitation(Array{""});
    checkVisitation(Array{Blob{}});
    checkVisitation(Array{Array{}});
    checkVisitation(Array{Object{}});
    checkVisitation(Object{ {"",null} });
    checkVisitation(Object{ {"",true} });
    checkVisitation(Object{ {"",false} });
    checkVisitation(Object{ {"",0} });
    checkVisitation(Object{ {"",0u} });
    checkVisitation(Object{ {"",0.0} });
    checkVisitation(Object{ {"",""} });
    checkVisitation(Object{ {"",Blob{}} });
    checkVisitation(Object{ {"",Array{}} });
    checkVisitation(Object{ {"",Object{}} });
}
GIVEN( "assorted pairs of variants" )
{
    checkBinaryVisitation(false);
    checkBinaryVisitation(true);
    checkBinaryVisitation(intMin);
    checkBinaryVisitation(intMax);
    checkBinaryVisitation(uintMax);
    checkBinaryVisitation(realMin);
    checkBinaryVisitation(realMax);
    checkBinaryVisitation(String("null"));
    checkBinaryVisitation(String("true"));
    checkBinaryVisitation(String("false"));
    checkBinaryVisitation(String("0"));
    checkBinaryVisitation(String("1"));
    checkBinaryVisitation(Blob{0x00});
    checkBinaryVisitation(Blob{0x00, 0x01, 0x02});
    checkBinaryVisitation(Array{null});
    checkBinaryVisitation(Array{true});
    checkBinaryVisitation(Array{false});
    checkBinaryVisitation(Array{0});
    checkBinaryVisitation(Array{0u});
    checkBinaryVisitation(Array{0.0});
    checkBinaryVisitation(Array{""});
    checkBinaryVisitation(Array{Array{}});
    checkBinaryVisitation(Array{Object{}});
    checkBinaryVisitation(Object{ {"",null} });
    checkBinaryVisitation(Object{ {"",true} });
    checkBinaryVisitation(Object{ {"",false} });
    checkBinaryVisitation(Object{ {"",0} });
    checkBinaryVisitation(Object{ {"",0u} });
    checkBinaryVisitation(Object{ {"",0.0} });
    checkBinaryVisitation(Object{ {"",""} });
    checkBinaryVisitation(Object{ {"",Array{}} });
    checkBinaryVisitation(Object{ {"",Object{}} });
}
}

#endif // #if CPPWAMP_TESTING_VARIANT
