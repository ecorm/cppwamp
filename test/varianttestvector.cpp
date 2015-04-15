/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#if CPPWAMP_TESTING_VARIANT

#include <tuple>
#include <vector>
#include <catch.hpp>
#include <cppwamp/variant.hpp>

using namespace wamp;

namespace
{

template <typename T>
void checkVec(const std::vector<T>& vector, bool convertible = true)
{
    using Vector = std::vector<T>;

    Array a(vector.cbegin(), vector.cend());
    Variant expected(a);
    INFO( "For vector " << expected );

    {
        Variant v;
        v = vector;
        CHECK( v == expected );
        CHECK( v.size() == vector.size() );
        for (Variant::SizeType i=0; i<v.size(); ++i)
        {
            CHECK( v[i] == vector.at(i) );
        }
        CHECK( v.convertsTo<Vector>() == convertible );
        Vector converted;
        if (convertible)
        {
            CHECK_NOTHROW( v.to(converted) );
            CHECK( converted == vector);
        }
        else
        {
            CHECK_THROWS_AS( v.to(converted), error::Conversion );
            CHECK( converted.empty() );
        }
    }

    {
        auto vec = vector;
        Variant v;
        v = std::move(vec);
        CHECK( v == expected );
    }
}

template <typename T>
void checkBadConversionTo(const Variant& v)
{
    INFO( "For variant " << v );
    using Vector = std::vector<T>;
    CHECK_FALSE( v.convertsTo<Vector>() );
    CHECK_THROWS_AS( v.to<Vector>(), error::Conversion );
    Vector vec;
    CHECK_THROWS_AS( v.to(vec), error::Conversion );
}

//------------------------------------------------------------------------------
template <typename TLower, typename TGreater>
bool differs(const TLower& lower, const TGreater& greater)
{
    using V = Variant;
    bool b[10] = {false};
    int i = 0;

    CHECK(( b[i++]= !(V(lower)   != lower) ));
    CHECK(( b[i++]= !(V(lower)   == greater) ));
    CHECK(( b[i++]=   V(lower)   != greater ));
    CHECK(( b[i++]= !(V(greater) == lower) ));
    CHECK(( b[i++]=   V(greater) != lower ));

    CHECK(( b[i++]= !(lower   != V(lower)) ));
    CHECK(( b[i++]= !(lower   == V(greater)) ));
    CHECK(( b[i++]=   lower   != V(greater) ));
    CHECK(( b[i++]= !(greater == V(lower)) ));
    CHECK(( b[i++]=   greater != V(lower) ));

    for (auto result: b)
        if (!result)
            return false;
    return true;
}

//------------------------------------------------------------------------------
template <typename TLeft, typename TRight>
bool same(const TLeft& lhs, const TRight& rhs)
{
    using V = Variant;
    bool b[8] = {false};
    int i = 0;

    CHECK(( b[i++]= !(V(lhs) != rhs) ));
    CHECK(( b[i++]=   V(lhs) == rhs ));
    CHECK(( b[i++]= !(V(rhs) != lhs) ));
    CHECK(( b[i++]=   V(rhs) == lhs ));

    CHECK(( b[i++]= !(lhs != V(rhs)) ));
    CHECK(( b[i++]=   lhs == V(rhs) ));
    CHECK(( b[i++]= !(rhs != V(lhs)) ));
    CHECK(( b[i++]=   rhs == V(lhs) ));

    for (auto result: b)
        if (!result)
            return false;
    return true;
}

} // namespace anonymous

//------------------------------------------------------------------------------
SCENARIO( "Variants initialized with vectors", "[Variant]" )
{
GIVEN( "an assortment of vectors of valid types" )
{
    checkVec<Null>({null});
    checkVec<Null>({null, null});
    checkVec<Bool>({false});
    checkVec<Bool>({true});
    checkVec<Bool>({false, true});
    checkVec<UInt>({0u});
    checkVec<UInt>({1u, 2u, 3u});
    checkVec<Int>({0});
    checkVec<Int>({-1, -2, -3});
    checkVec<Real>({0.0});
    checkVec<Real>({0.0, 1.1, 2.2});
    checkVec<unsigned>({1, 2, 3});
    checkVec<int>({-1, -2, -3});
    checkVec<unsigned short>({1, 2, 3});
    checkVec<short>({-1, -2, -3});
    checkVec<float>({0.0f, 1.1f, 2.2f});
    checkVec<String>({""});
    checkVec<String>({"One", "Two", "Three"});
    checkVec<const char*>({"One", "Two", "Three"}, false);
    checkVec<Array>({{"foo", 42}, {null, false}});
    checkVec<std::vector<int>>({{1, 2, 3}, {4, 5, 6}});
    checkVec<Object>({ {{"one", 1}}, {{"two", 2.0}, {"three", 3u}} });
    checkVec<std::map<String, int>>({ {{"one", 1}},
                                       {{"two", 2}, {"three", 3}} });
}
GIVEN( "an assortment of valid empty vectors" )
{
    checkVec<Null>({});
    checkVec<Bool>({});
    checkVec<UInt>({});
    checkVec<Int>({});
    checkVec<Real>({});
    checkVec<unsigned>({});
    checkVec<int>({});
    checkVec<unsigned short>({});
    checkVec<short>({});
    checkVec<float>({});
    checkVec<String>({});
    checkVec<const char*>({});
    checkVec<Array>({});
    checkVec<std::vector<int>>({});
    checkVec<std::vector<int>>({{}});
    checkVec<Object>({});
    checkVec<std::map<String, int>>({});
}
// The following should cause 4 static assertion failures:
//GIVEN( "a vector with an invalid type" )
//{
//    struct Invalid {};
//    std::vector<Invalid> vec{Invalid{}};
//    Variant a(vec);
//    Variant b(std::move(vec));
//    Variant c; c = vec;
//    Variant d; d = std::move(vec);
//}
}

SCENARIO( "Invalid variant conversion to vector", "[Variant]" )
{
GIVEN( "invalid vector types" )
{
    struct Foo {};
    checkBadConversionTo<bool>(Variant{true});
    checkBadConversionTo<int>(Variant{Array{"Hello"}});
    checkBadConversionTo<Foo>(Variant{Array{42}});
    checkBadConversionTo<Null>(Variant{Array{0}});
}
}

SCENARIO( "Comparing variants to vectors", "[Variant]" )
{
    using std::vector;

    WHEN( "one side is empty" )
    {
        CHECK( differs(vector<Null>{},   vector<Null>{null}) );
        CHECK( differs(vector<Bool>{},   vector<Bool>{false}) );
        CHECK( differs(vector<Int>{},    vector<Int>{0}) );
        CHECK( differs(vector<UInt>{},   vector<UInt>{0u}) );
        CHECK( differs(vector<Real>{},   vector<Real>{0.0}) );
        CHECK( differs(vector<String>{}, vector<String>{""}) );
        CHECK( differs(vector<Array>{},  vector<Array>{Array{}}) );
        CHECK( differs(vector<Object>{}, vector<Object>{Object{}}) );
    }
    WHEN ( "performing lexicographical comparisons" )
    {
        using V = vector<Int>;
        CHECK( differs(V{0},     V{1}) );
        CHECK( differs(V{-1},    V{0}) );
        CHECK( differs(V{0},     V{0,0}) );
        CHECK( differs(V{1},     V{1,0}) );
        CHECK( differs(V{1},     V{1,1}) );
        CHECK( differs(V{0,0},   V{1}) );
        CHECK( differs(V{0,0},   V{0,1}) );
        CHECK( differs(V{0,0},   V{1,0}) );
        CHECK( differs(V{0,0},   V{1,1}) );
        CHECK( differs(V{0,1},   V{1,0}) );
        CHECK( differs(V{0,1},   V{1,1}) );
        CHECK( differs(V{1,0},   V{1,1}) );
        CHECK( differs(V{1,0,0}, V{1,1}) );
    }
    WHEN ( "elements are of numeric type" )
    {
        CHECK( same   (vector<Int>{0},    vector<UInt>{0u}) );
        CHECK( same   (vector<Int>{0},    vector<Real>{0.0}) );
        CHECK( same   (vector<UInt>{0u},  vector<Real>{0.0}) );
        CHECK( same   (vector<Int>{-1},   vector<Real>{-1.0}) );
        CHECK( differs(vector<Int>{0},    vector<UInt>{1u}) );
        CHECK( differs(vector<Int>{0},    vector<Real>{1.0}) );
        CHECK( differs(vector<Int>{0},    vector<Real>{0.1}) );
        CHECK( differs(vector<Int>{-1},   vector<Int>{0}) );
        CHECK( differs(vector<UInt>{0u},  vector<Int>{-1}) ); // Signed/unsigned comparison
        CHECK( differs(vector<Int>{-1},   vector<Real>{0.0}) );
        CHECK( differs(vector<Int>{-1},   vector<Real>{-0.9}) );
        CHECK( differs(vector<UInt>{0u},  vector<Int>{1}) );
        CHECK( differs(vector<UInt>{0u},  vector<Real>{1.0}) );
        CHECK( differs(vector<UInt>{0u},  vector<Real>{0.1}) );
        CHECK( differs(vector<Real>{0.0}, vector<Int>{1}) );
        CHECK( differs(vector<Real>{0.0}, vector<UInt>{1u}) );
    }
}

#endif // #if CPPWAMP_TESTING_VARIANT
