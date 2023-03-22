/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include <algorithm>
#include <tuple>
#include <vector>
#include <catch2/catch.hpp>
#include <cppwamp/types/array.hpp>
#include <cppwamp/types/set.hpp>
#include <cppwamp/types/tuple.hpp>
#include <cppwamp/types/unorderedmap.hpp>
#include <cppwamp/types/unorderedset.hpp>

using namespace wamp;

namespace
{

//------------------------------------------------------------------------------
template <typename T>
using VariantConstructionOp = decltype(Variant(std::declval<T>()));

//------------------------------------------------------------------------------
template <typename T>
using VariantAssignOp = decltype(std::declval<Variant>() = std::declval<T>());

//------------------------------------------------------------------------------
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
            CHECK( v.at(i) == vector.at(i) );
        }
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

    {
        Variant v(vector);
        CHECK( v == expected );
    }
}

//------------------------------------------------------------------------------
template <typename T>
void checkBadConversionToVector(const Variant& v)
{
    INFO( "For variant " << v );
    using Vector = std::vector<T>;
    CHECK_THROWS_AS( v.to<Vector>(), error::Conversion );
    Vector vec;
    CHECK_THROWS_AS( v.to(vec), error::Conversion );
}

//------------------------------------------------------------------------------
template <typename TLower, typename TGreater>
bool vectorsDiffer(const TLower& lower, const TGreater& greater)
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
bool vectorsAreSame(const TLeft& lhs, const TRight& rhs)
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

//------------------------------------------------------------------------------
template <typename T>
void checkMap(const std::map<String, T>& map, bool convertible = true)
{
    using Map = std::map<String, T>;

    Object o(map.cbegin(), map.cend());
    Variant expected(o);
    INFO( "For map " << expected );

    {
        Variant v;
        v = map;
        CHECK( v == expected );
        CHECK( v.size() == map.size() );
        for (const auto& kv: map)
        {
            CHECK( v[kv.first] == kv.second );
            CHECK( v.at(kv.first) == kv.second );
        }
        Map converted;
        if (convertible)
        {
            CHECK_NOTHROW( v.to(converted) );
            CHECK( converted == map);
        }
        else
        {
            CHECK_THROWS_AS( v.to(converted), error::Conversion );
            CHECK( converted.empty() );
        }
    }

    {
        auto map2 = map;
        Variant v;
        v = std::move(map2);
        CHECK( v == expected );
    }
}

//------------------------------------------------------------------------------
template <typename T>
void checkBadConversionToMap(const Variant& v)
{
    INFO( "For variant " << v );
    using Map = std::map<String, T>;
    CHECK_THROWS_AS( v.to<Map>(), error::Conversion );
    Map map;
    CHECK_THROWS_AS( v.to(map), error::Conversion );
}

//------------------------------------------------------------------------------
template <typename TLower, typename TGreater>
bool mapsDiffer(const TLower& lower, const TGreater& greater)
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
bool mapsAreSame(const TLeft& lhs, const TRight& rhs)
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

} // anonymous namespace


//------------------------------------------------------------------------------
SCENARIO( "Converting to/from std::unordered_map", "[Variant]" )
{
GIVEN( "a valid variant object type" )
{
    Variant v(Object{{"a", 1},{"b", 2}});
    WHEN( "converting to a std::unordered_map" )
    {
        auto map = v.to<std::unordered_map<String, int>>();
        THEN( "the map is as expected" )
        {
            CHECK( map.size() == 2 );
            CHECK( map["a"] == 1 );
            CHECK( map["b"] == 2 );
        }
    }
}
GIVEN( "an empty variant object type" )
{
    Variant v(Object{});
    WHEN( "converting to a std::unordered_map" )
    {
        auto map = v.to<std::unordered_map<String, int>>();
        THEN( "the map is as expected" )
        {
            CHECK( map.empty() );
        }
    }
}
GIVEN( "a valid std::unordered_map type" )
{
    std::unordered_map<String, int> map{{"a", 1},{"b", 2}};
    WHEN( "converting to a variant" )
    {
        auto v = Variant::from(map);
        THEN( "the variant is as expected" )
        {
            CHECK( v == (Object{{"a", 1},{"b", 2}}) );
        }
    }
}
GIVEN( "an empty std::unordered_map" )
{
    std::unordered_map<String, int> map{};
    WHEN( "converting to a variant" )
    {
        auto v = Variant::from(map);
        THEN( "the variant is as expected" )
        {
            REQUIRE( v.is<Object>() );
            CHECK( v.as<Object>().empty() );
        }
    }
}
GIVEN( "an invalid variant object type" )
{
    Variant v(Object{{"a", 1},{"b", null}});
    WHEN( "converting to a std::unordered_map" )
    {
        using MapType = std::unordered_map<String, int>;
        CHECK_THROWS_AS( v.to<MapType>(), error::Conversion );
    }
}
}


//------------------------------------------------------------------------------
SCENARIO( "Converting to/from std::array", "[Variant]" )
{
GIVEN( "a valid variant array type" )
{
    Variant v(Array{1, 3, 2});
    WHEN( "converting to a std::array" )
    {
        auto array = v.to<std::array<int, 3>>();
        THEN( "the array is as expected" )
        {
            REQUIRE( array.size() == 3 );
            auto iter = array.begin();
            CHECK( *iter++ == 1 );
            CHECK( *iter++ == 3 );
            CHECK( *iter++ == 2 );
        }
    }
}
GIVEN( "a variant array of 4 elements" )
{
    Variant v(Array{1, 2, 3, 4});
    WHEN( "converting to a std::array of 3 elements" )
    {
        using ArraytType = std::array<int, 3>;
        CHECK_THROWS_AS( v.to<ArraytType>(), error::Conversion );
    }
}
GIVEN( "a valid std::array type" )
{
    std::array<String, 3> array{"a", "b", "c"};
    WHEN( "converting to a variant" )
    {
        auto v = Variant::from(array);
        THEN( "the variant is as expected" )
        {
            CHECK( v == (Array{"a", "b", "c"}) );
        }
    }
}
GIVEN( "an invalid variant array type" )
{
    Variant v(Array{"a", null});
    WHEN( "converting to a std::array" )
    {
        using ArrayType = std::array<String, 3>;
        CHECK_THROWS_AS( v.to<ArrayType>(), error::Conversion );
    }
}
}


//------------------------------------------------------------------------------
SCENARIO( "Converting to/from std::set", "[Variant]" )
{
GIVEN( "a valid variant array type" )
{
    Variant v(Array{1, 3, 2});
    WHEN( "converting to a std::set" )
    {
        auto set = v.to<std::set<int>>();
        THEN( "the set is as expected" )
        {
            REQUIRE( set.size() == 3 );
            auto iter = set.begin();
            CHECK( *iter++ == 1 );
            CHECK( *iter++ == 2 );
            CHECK( *iter++ == 3 );
        }
    }
}
GIVEN( "an empty variant array type" )
{
    Variant v(Array{});
    WHEN( "converting to a std::set" )
    {
        auto set = v.to<std::set<int>>();
        THEN( "the set is as expected" )
        {
            CHECK( set.empty() );
        }
    }
}
GIVEN( "a valid std::set type" )
{
    std::set<String> set{"a", "b", "c"};
    WHEN( "converting to a variant" )
    {
        auto v = Variant::from(set);
        THEN( "the variant is as expected" )
        {
            CHECK( v == (Array{"a", "b", "c"}) );
        }
    }
}
GIVEN( "an empty std::set" )
{
    std::set<String> set{};
    WHEN( "converting to a variant" )
    {
        auto v = Variant::from(set);
        THEN( "the variant is as expected" )
        {
            REQUIRE( v.is<Array>() );
            CHECK( v.as<Array>().empty() );
        }
    }
}
GIVEN( "an invalid variant array type" )
{
    Variant v(Array{"a", null});
    WHEN( "converting to a std::set" )
    {
        using SetType = std::set<String>;
        CHECK_THROWS_AS( v.to<SetType>(), error::Conversion );
    }
}
}


//------------------------------------------------------------------------------
SCENARIO( "Converting to/from std::unordered_set", "[Variant]" )
{
GIVEN( "a valid variant array type" )
{
    Variant v(Array{1, 3, 2});
    WHEN( "converting to a std::unordered_set" )
    {
        auto set = v.to<std::unordered_set<int>>();
        THEN( "the set is as expected" )
        {
            REQUIRE( set.size() == 3 );
            std::set<int> sorted(set.begin(), set.end());
            auto iter = sorted.begin();
            CHECK( *iter++ == 1 );
            CHECK( *iter++ == 2 );
            CHECK( *iter++ == 3 );
        }
    }
}
GIVEN( "an empty variant array type" )
{
    Variant v(Array{});
    WHEN( "converting to a std::unordered_set" )
    {
        auto set = v.to<std::unordered_set<int>>();
        THEN( "the set is as expected" )
        {
            CHECK( set.empty() );
        }
    }
}
GIVEN( "a valid std::unordered_set type" )
{
    std::unordered_set<String> set{"a", "b", "c"};
    WHEN( "converting to a variant" )
    {
        auto v = Variant::from(set);
        THEN( "the variant is as expected" )
        {
            auto array = v.as<Array>();
            std::sort(array.begin(), array.end());
            CHECK( array == (Array{"a", "b", "c"}) );
        }
    }
}
GIVEN( "an empty std::unordered_set" )
{
    std::unordered_set<String> set{};
    WHEN( "converting to a variant" )
    {
        auto v = Variant::from(set);
        THEN( "the variant is as expected" )
        {
            REQUIRE( v.is<Array>() );
            CHECK( v.as<Array>().empty() );
        }
    }
}
GIVEN( "an invalid variant array type" )
{
    Variant v(Array{"a", null});
    WHEN( "converting to a std::unordered_set" )
    {
        using SetType = std::unordered_set<String>;
        CHECK_THROWS_AS( v.to<SetType>(), error::Conversion );
    }
}
}

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
    checkVec<Blob>({Blob{}});
    checkVec<Blob>({Blob{0x00}, Blob{0x00, 0x01}, Blob{0x00, 0x01, 0x02}});
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
    checkVec<Blob>({});
    checkVec<Array>({});
    checkVec<std::vector<int>>({});
    checkVec<std::vector<int>>({{}});
    checkVec<Object>({});
    checkVec<std::map<String, int>>({});
}
GIVEN( "a vector with an invalid type" )
{
    struct Invalid {};
    using InvalidVector = std::vector<Invalid>;
    using ValidVector = std::vector<int>;

    CHECK(isDetected<VariantConstructionOp, ValidVector>());
    CHECK_FALSE(isDetected<VariantConstructionOp, InvalidVector>());

    CHECK(isDetected<VariantAssignOp, ValidVector>());
    CHECK_FALSE(isDetected<VariantAssignOp, InvalidVector>());
}
}

//------------------------------------------------------------------------------
SCENARIO( "Invalid variant conversion to vector", "[Variant]" )
{
GIVEN( "invalid vector types" )
{
    checkBadConversionToVector<bool>(Variant{true});
    checkBadConversionToVector<int>(Variant{Array{"Hello"}});
    checkBadConversionToVector<Null>(Variant{Array{0}});
}
}

//------------------------------------------------------------------------------
SCENARIO( "Comparing variants to vectors", "[Variant]" )
{
    using std::vector;

    WHEN( "one side is empty" )
    {
        CHECK( vectorsDiffer(vector<Null>{},   vector<Null>{null}) );
        CHECK( vectorsDiffer(vector<Bool>{},   vector<Bool>{false}) );
        CHECK( vectorsDiffer(vector<Int>{},    vector<Int>{0}) );
        CHECK( vectorsDiffer(vector<UInt>{},   vector<UInt>{0u}) );
        CHECK( vectorsDiffer(vector<Real>{},   vector<Real>{0.0}) );
        CHECK( vectorsDiffer(vector<String>{}, vector<String>{""}) );
        CHECK( vectorsDiffer(vector<Blob>{},   vector<Blob>{Blob{}}) );
        CHECK( vectorsDiffer(vector<Array>{},  vector<Array>{Array{}}) );
        CHECK( vectorsDiffer(vector<Object>{}, vector<Object>{Object{}}) );
    }
    WHEN ( "performing lexicographical comparisons" )
    {
        using V = vector<Int>;
        CHECK( vectorsAreSame(V{0},     V{0}) );
        CHECK( vectorsDiffer (V{0},     V{1}) );
        CHECK( vectorsDiffer (V{-1},    V{0}) );
        CHECK( vectorsDiffer (V{0},     V{0,0}) );
        CHECK( vectorsDiffer (V{1},     V{1,0}) );
        CHECK( vectorsDiffer (V{1},     V{1,1}) );
        CHECK( vectorsDiffer (V{0,0},   V{1}) );
        CHECK( vectorsDiffer (V{0,0},   V{0,1}) );
        CHECK( vectorsDiffer (V{0,0},   V{1,0}) );
        CHECK( vectorsDiffer (V{0,0},   V{1,1}) );
        CHECK( vectorsDiffer (V{0,1},   V{1,0}) );
        CHECK( vectorsDiffer (V{0,1},   V{1,1}) );
        CHECK( vectorsDiffer (V{1,0},   V{1,1}) );
        CHECK( vectorsDiffer (V{1,0,0}, V{1,1}) );
    }
    WHEN ( "boolean-related value comparisons" )
    {
        CHECK( vectorsAreSame(vector<Bool>{false}, vector<Bool>{false}) );
        CHECK( vectorsAreSame(vector<Bool>{true},  vector<Bool>{true}) );
        CHECK( vectorsDiffer (vector<Bool>{false}, vector<Bool>{true}) );
        CHECK( vectorsDiffer (vector<Bool>{false}, vector<Int>{0}) );
        CHECK( vectorsDiffer (vector<Bool>{true},  vector<Int>{1}) );
    }
    WHEN ( "elements are of numeric type" )
    {
        CHECK( vectorsAreSame(vector<Int>{0},    vector<UInt>{0u}) );
        CHECK( vectorsAreSame(vector<Int>{0},    vector<Real>{0.0}) );
        CHECK( vectorsAreSame(vector<UInt>{0u},  vector<Real>{0.0}) );
        CHECK( vectorsAreSame(vector<Int>{-1},   vector<Real>{-1.0}) );
        CHECK( vectorsDiffer (vector<Int>{0},    vector<UInt>{1u}) );
        CHECK( vectorsDiffer (vector<Int>{0},    vector<Real>{1.0}) );
        CHECK( vectorsDiffer (vector<Int>{0},    vector<Real>{0.1}) );
        CHECK( vectorsDiffer (vector<Int>{-1},   vector<Int>{0}) );
        CHECK( vectorsDiffer (vector<UInt>{0u},  vector<Int>{-1}) ); // Signed/unsigned comparison
        CHECK( vectorsDiffer (vector<Int>{-1},   vector<Real>{0.0}) );
        CHECK( vectorsDiffer (vector<Int>{-1},   vector<Real>{-0.9}) );
        CHECK( vectorsDiffer (vector<UInt>{0u},  vector<Int>{1}) );
        CHECK( vectorsDiffer (vector<UInt>{0u},  vector<Real>{1.0}) );
        CHECK( vectorsDiffer (vector<UInt>{0u},  vector<Real>{0.1}) );
        CHECK( vectorsDiffer (vector<Real>{0.0}, vector<Int>{1}) );
        CHECK( vectorsDiffer (vector<Real>{0.0}, vector<UInt>{1u}) );
    }
}

//------------------------------------------------------------------------------
SCENARIO( "Variants initialized with maps", "[Variant]" )
{
GIVEN( "an assortment of maps of valid types" )
{
    checkMap<Null>({ {"", null} });
    checkMap<Null>({ {"key1", null}, {"key2", null} });
    checkMap<Bool>({ {"key", false} });
    checkMap<Bool>({ {"key", true} });
    checkMap<Bool>({ {"key1", false}, {"key2", true} });
    checkMap<UInt>({ {"key1", 0u} });
    checkMap<UInt>({ {"key1", 1u}, {"key2", 2u}, {"key3", 3u} });
    checkMap<Int>({ {"key1", 0} });
    checkMap<Int>({ {"key1", -1}, {"key2", -2}, {"key3", -3} });
    checkMap<Real>({ {"key", 0.0} });
    checkMap<Real>({ {"key1", 1.1}, {"key2", 2.2}, {"key3", 3.3} });
    checkMap<unsigned>({ {"key1", 1}, {"key2", 2}, {"key3", 3} });
    checkMap<int>({ {"key1", -1}, {"key2", -2}, {"key3", -3} });
    checkMap<unsigned short>({ {"key1", 1}, {"key2", 2}, {"key3", 3} });
    checkMap<short>({ {"key1", -1}, {"key2", -2}, {"key3", -3} });
    checkMap<float>({ {"key1", 1.1f}, {"key2", 2.2f}, {"key3", 3.3f} });
    checkMap<String>({ {"", ""} });
    checkMap<String>({ {"key", ""} });
    checkMap<String>({ {"key1", "One"}, {"key2", "Two"}, {"key3", "Three"} });
    checkMap<const char*>({ {"key1", "One"}, {"key2", "Two"} }, false);
    checkMap<Blob>({ {"", Blob{}} });
    checkMap<Blob>({ {"key", Blob{0x42}} });
    checkMap<Array>({ {"key1", {"foo", 42}}, {"key2", {null, false}} });
    checkMap<std::vector<int>>({ {"key1", {1, 2, 3}}, {"key2", {4, 5, 6}} });
    checkMap<Object>({ {"key1", {{"one", 1}}}, {"key2", {{"two", 2.0}}} });
    checkMap<std::map<String, int>>({ {"key1", {{"one", 1}}},
                                      {"key2", {{"two", 2}}} });
}
GIVEN( "an assortment of valid empty maps" )
{
    checkMap<Null>({});
    checkMap<Bool>({});
    checkMap<UInt>({});
    checkMap<Int>({});
    checkMap<Real>({});
    checkMap<unsigned>({});
    checkMap<int>({});
    checkMap<unsigned short>({});
    checkMap<short>({});
    checkMap<float>({});
    checkMap<String>({});
    checkMap<const char*>({});
    checkMap<Blob>({});
    checkMap<Array>({});
    checkMap<std::vector<int>>({});
    checkMap<std::vector<int>>({{}});
    checkMap<Object>({});
    checkMap<std::map<String, int>>({});
}
GIVEN( "a map with an invalid type" )
{
    struct Invalid {};
    using InvalidMap = std::map<String, Invalid>;
    using InvalidKeyMap = std::map<int, int>;
    using ValidMap = std::map<String, int>;

    CHECK(isDetected<VariantConstructionOp, ValidMap>());
    CHECK_FALSE(isDetected<VariantConstructionOp, InvalidMap>());
    CHECK_FALSE(isDetected<VariantConstructionOp, InvalidKeyMap>());

    CHECK(isDetected<VariantAssignOp, ValidMap>());
    CHECK_FALSE(isDetected<VariantAssignOp, InvalidMap>());
    CHECK_FALSE(isDetected<VariantAssignOp, InvalidKeyMap>());
}
}

//------------------------------------------------------------------------------
SCENARIO( "Invalid variant conversion to map", "[Variant]" )
{
GIVEN( "invalid map types" )
{
    checkBadConversionToMap<bool>(Variant{true});
    checkBadConversionToMap<int>(Variant{Object{ {"key", "Hello"} }});
    checkBadConversionToMap<Null>(Variant{Object{ {"", 0} }});
}
}

//------------------------------------------------------------------------------
SCENARIO( "Comparing variants to maps", "[Variant]" )
{
    using std::map;
    using S = String;

    WHEN( "one side is empty" )
    {
        CHECK( mapsDiffer(map<S,Null>{},   map<S,Null>{ {"", null} }) );
        CHECK( mapsDiffer(map<S,Bool>{},   map<S,Bool>{ {"", false} }) );
        CHECK( mapsDiffer(map<S,Int>{},    map<S,Int>{ {"", 0} }) );
        CHECK( mapsDiffer(map<S,UInt>{},   map<S,UInt>{ {"", 0u} }) );
        CHECK( mapsDiffer(map<S,Real>{},   map<S,Real>{ {"", 0.0} }) );
        CHECK( mapsDiffer(map<S,String>{}, map<S,String>{ {"", ""} }) );
        CHECK( mapsDiffer(map<S,Blob>{},   map<S,Blob>{ {"", Blob{}} }) );
        CHECK( mapsDiffer(map<S,Array>{},  map<S,Array>{ {"", Array{}} }) );
        CHECK( mapsDiffer(map<S,Object>{}, map<S,Object>{ {"", Object{}} }) );
    }
    WHEN( "both sides have a single, identical key" )
    {
        CHECK( mapsDiffer(map<S,Bool>{ {"k", false} },   map<S,Bool>{ {"k", true} }) );
        CHECK( mapsDiffer(map<S,Int>{ {"k", -1} },       map<S,Int>{ {"k", 0} }) );
        CHECK( mapsDiffer(map<S,UInt>{ {"k", 0u} },      map<S,UInt>{ {"k", 1u} }) );
        CHECK( mapsDiffer(map<S,Real>{ {"k", 0.0} },     map<S,Real>{ {"k", 1.0} }) );
        CHECK( mapsDiffer(map<S,String>{ {"k", "A"} },   map<S,String>{ {"k", "B"} }) );
        CHECK( mapsDiffer(map<S,Blob>{ {"k", Blob{0}} }, map<S,Blob>{ {"k", Blob{1}} }) );
        CHECK( mapsDiffer(map<S,Array>{ {"k", Array{}}},
                          map<S,Array>{ {"k", Array{null}}}) );
        CHECK( mapsDiffer(map<S,Object>{ {"k", Object{}} },
                          map<S,Object>{ {"k", Object{{"",null}}} }) );
    }
    WHEN ( "performing lexicographical comparisons on only the key" )
    {
        using Map = std::map<String, Null>;
        CHECK( mapsDiffer(Map{ {"A", null} },  Map{ {"AA", null} }) );
        CHECK( mapsDiffer(Map{ {"A", null} },  Map{ {"B",  null} }) );
        CHECK( mapsDiffer(Map{ {"A", null} },  Map{ {"a",  null} }) );
        CHECK( mapsDiffer(Map{ {"B", null} },  Map{ {"BA", null} }) );
        CHECK( mapsDiffer(Map{ {"B", null} },  Map{ {"a",  null} }) );
    }
    WHEN ( "performing lexicographical comparisons on both key and value" )
    {
        CHECK( mapsDiffer(map<S,Bool>{ {"A", true} },     map<S,Bool>{ {"AA", false} }) );
        CHECK( mapsDiffer(map<S,Int>{ {"A", 0} },         map<S,Int>{ {"B",  -1} }) );
        CHECK( mapsDiffer(map<S,String>{ {"A", "a"} },    map<S,String>{ {"a", "A"} }) );
        CHECK( mapsDiffer(map<S,Blob>{ {" A", Blob{1}} }, map<S,Blob>{ {"A",  Blob{0}} }) );
        CHECK( mapsDiffer(map<S,Array>{ {"B", Array{null}} },
                          map<S,Array>{ {"BA", Array{}} }) );
        CHECK( mapsDiffer(map<S,Object>{ {"B", Object{{"",null}}} },
                          map<S,Object>{ {"a", Object{}} }) );
    }
    WHEN ( "elements are of numeric type" )
    {
        CHECK( mapsAreSame(map<S,Int>{ {"", 0} },   map<S,UInt>{ {"", 0u} }) );
        CHECK( mapsAreSame(map<S,Int>{ {"", 0} },   map<S,Real>{ {"", 0.0} }) );
        CHECK( mapsAreSame(map<S,UInt>{ {"", 0u} }, map<S,Real>{ {"", 0.0} }) );
        CHECK( mapsAreSame(map<S,Int>{ {"", -1} },  map<S,Real>{ {"", -1.0} }) );
        CHECK( mapsDiffer (map<S,Int>{ {"", 0} },   map<S,UInt>{ {"", 1u} }) );
        CHECK( mapsDiffer (map<S,Int>{ {"", 0} },   map<S,Real>{ {"", 1.0} }) );
        CHECK( mapsDiffer (map<S,Int>{ {"", 0} },   map<S,Real>{ {"", 0.1} }) );
        CHECK( mapsDiffer (map<S,Int>{ {"", -1} },  map<S,Int>{ {"", 0} }) );

        // Signed/unsigned comparison
        CHECK( mapsDiffer(map<S,UInt>{ {"", 0u} },  map<S,Int>{ {"", -1} }) );

        CHECK( mapsDiffer(map<S,Int>{ {"", -1} },   map<S,Real>{ {"", 0.0} }) );
        CHECK( mapsDiffer(map<S,Int>{ {"", -1} },   map<S,Real>{ {"", -0.9} }) );
        CHECK( mapsDiffer(map<S,UInt>{ {"", 0u} },  map<S,Int>{ {"", 1} }) );
        CHECK( mapsDiffer(map<S,UInt>{ {"", 0u} },  map<S,Real>{ {"", 1.0} }) );
        CHECK( mapsDiffer(map<S,UInt>{ {"", 0u} },  map<S,Real>{ {"", 0.1} }) );
        CHECK( mapsDiffer(map<S,Real>{ {"", 0.0} }, map<S,Int>{ {"", 1} }) );
        CHECK( mapsDiffer(map<S,Real>{ {"", 0.0} }, map<S,UInt>{ {"", 1u} }) );
    }
}

//------------------------------------------------------------------------------
SCENARIO( "Variant initialization from a tuple", "[Variant]" )
{
GIVEN( "a tuple of valid types" )
{
    auto tuple = std::make_tuple(null, false, true, 0u, -1, 42.0, "foo",
                                 Blob{0x42}, Array{"a", 123}, Object{{"o", 321}},
                                 std::make_tuple("b", 124));
    Variant expected = Array{null, false, true, 0u, -1, 42.0, "foo",
                             Blob{0x42}, Array{"a", 123}, Object{{"o", 321}},
                             Array{"b", 124}};
    WHEN( "a variant is constructed from the tuple" )
    {
        auto v = Variant::from(tuple);
        CHECK( v == expected );
    }
    WHEN( "the tuple is assigned to a variant" )
    {
        Variant v;
        v = toArray(tuple);
        CHECK( v == expected );
    }
}
GIVEN( "an empty tuple" )
{
    std::tuple<> tuple;
    Variant expected = Array{};
    WHEN( "a variant is constructed from the tuple" )
    {
        auto v = Variant::from(tuple);
        CHECK( v == expected );
    }
    WHEN( "the tuple is assigned to a variant" )
    {
        Variant v;
        v = toArray(tuple);
        CHECK( v == expected );
    }
}
}

//------------------------------------------------------------------------------
SCENARIO( "Variant conversion/comparison to tuple", "[Variant]" )
{
    GIVEN( "a tuple of valid types" )
    {
        auto tuple = std::make_tuple(null, false, true, 0u, -1, 42.0,
                String("foo"), Blob{0x42}, Array{"a", 123}, Object{{"o", 321}});
        using TupleType = decltype(tuple);

        WHEN( "a matching variant is converted to the tuple" )
        {
            auto v = Variant::from(tuple);
            TupleType result;
            REQUIRE_NOTHROW( v.to(result) );
            CHECK(( result == tuple ));
            CHECK(( v == tuple ));
            CHECK(( v.as<Array>() == tuple ));
            CHECK_FALSE(( v != tuple ));
            CHECK_FALSE(( v.as<Array>() != tuple ));
        }
        WHEN( "a matching variant differs by only one value" )
        {
            auto v = Variant::from(tuple);
            v.as<Array>().at(3).as<UInt>() = 666u;
            CHECK_FALSE(( v == tuple ));
            CHECK_FALSE(( v.as<Array>() == tuple ));
            CHECK(( v != tuple ));
            CHECK(( v.as<Array>() != tuple ));
        }
    }
    GIVEN( "a tuple of convertible types" )
    {
        auto tuple = std::make_tuple(false, 3, 42.0);
        using TupleType = decltype(tuple);

        WHEN( "a compatible variant is converted to the tuple" )
        {
            Variant v(Array{0, 3u, 42});
            TupleType result;
            REQUIRE_NOTHROW( v.to(result) );
            CHECK(( result == tuple ));
            result = TupleType();
            REQUIRE_NOTHROW( toTuple(v.as<Array>(), result) );
            CHECK(( result == tuple ));
        }
        WHEN( "a compatible variant is compared to the tuple" )
        {
            Variant v(Array{false, 3u, 42});
            CHECK(( v == tuple ));
            CHECK(( v.as<Array>() == tuple ));
            CHECK_FALSE(( v != tuple ));
            CHECK_FALSE(( v.as<Array>() != tuple ));
        }
        WHEN( "a compatible variant differs by only one value" )
        {
            Variant v(Array{false, 3u, 41});
            CHECK_FALSE(( v == tuple ));
            CHECK_FALSE(( v.as<Array>() == tuple ));
            CHECK(( v != tuple ));
            CHECK(( v.as<Array>() != tuple ));
        }
    }
    GIVEN( "an empty tuple" )
    {
        std::tuple<> tuple;
        using TupleType = decltype(tuple);

        WHEN( "an empty array variant is converted to the tuple" )
        {
            Variant v(Array{});
            TupleType result;
            REQUIRE_NOTHROW( v.to(result) );
            CHECK(( result == tuple ));
            result = TupleType();
            REQUIRE_NOTHROW( toTuple(v.as<Array>(), result) );
            CHECK(( result == tuple ));
            CHECK(( v == tuple ));
            CHECK(( v.as<Array>() == tuple ));
            CHECK_FALSE(( v != tuple ));
            CHECK_FALSE(( v.as<Array>() != tuple ));
        }
        WHEN( "a non-empty array variant is converted to the tuple" )
        {
            Variant v(Array{null});
            TupleType result;
            REQUIRE_THROWS_AS( v.to(result), error::Conversion );
            REQUIRE_THROWS_AS( toTuple(v.as<Array>(), result),
                               error::Conversion );
            CHECK_FALSE(( v == tuple ));
            CHECK_FALSE(( v.as<Array>() == tuple ));
            CHECK(( v != tuple ));
            CHECK(( v.as<Array>() != tuple ));
        }
        WHEN( "a null variant is compared to the tuple" )
        {
            Variant v;
            CHECK_FALSE(( v == tuple ));
            CHECK(( v != tuple ));
        }
    }
    GIVEN( "a wrongly-sized tuple type" )
    {
        auto tuple = std::make_tuple(true, Int(42));
        using TupleType = decltype(tuple);

        WHEN( "an array Variant is narrower than the tuple" )
        {
            Variant v(Array{true});
            TupleType result;
            REQUIRE_THROWS_AS( v.to(result), error::Conversion );
            REQUIRE_THROWS_AS( toTuple(v.as<Array>(), result),
                               error::Conversion );
            CHECK_FALSE(( v == tuple ));
            CHECK_FALSE(( v.as<Array>() == tuple ));
            CHECK(( v != tuple ));
            CHECK(( v.as<Array>() != tuple ));
        }
        WHEN( "an array Variant is wider than the tuple" )
        {
            Variant v(Array{true, 42, null});
            TupleType result;
            REQUIRE_THROWS_AS( v.to(result), error::Conversion );
            REQUIRE_THROWS_AS( toTuple(v.as<Array>(), result),
                               error::Conversion );
            CHECK_FALSE(( v == tuple ));
            CHECK_FALSE(( v.as<Array>() == tuple ));
            CHECK(( v != tuple ));
            CHECK(( v.as<Array>() != tuple ));
        }
    }
    GIVEN( "a correctly-sized tuple with unconvertible types" )
    {
        auto tuple = std::make_tuple(null, true, Int(42));
        using TupleType = decltype(tuple);

        WHEN( "a Variant is converted to a mismatched tuple" )
        {
            Variant v(Array{true, null, 42});
            TupleType result;
            REQUIRE_THROWS_AS( v.to(result), error::Conversion );
            REQUIRE_THROWS_AS( toTuple(v.as<Array>(), result),
                               error::Conversion );
            CHECK_FALSE(( v == tuple ));
            CHECK_FALSE(( v.as<Array>() == tuple ));
            CHECK(( v != tuple ));
            CHECK(( v.as<Array>() != tuple ));
        }
    }
}
