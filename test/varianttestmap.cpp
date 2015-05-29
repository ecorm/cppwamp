/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#if CPPWAMP_TESTING_VARIANT

#include <map>
#include <catch.hpp>
#include <cppwamp/variant.hpp>

using namespace wamp;

namespace
{

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

template <typename T>
void checkBadConversionTo(const Variant& v)
{
    INFO( "For variant " << v );
    using Map = std::map<String, T>;
    CHECK_THROWS_AS( v.to<Map>(), error::Conversion );
    Map map;
    CHECK_THROWS_AS( v.to(map), error::Conversion );
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
    checkMap<Array>({});
    checkMap<std::vector<int>>({});
    checkMap<std::vector<int>>({{}});
    checkMap<Object>({});
    checkMap<std::map<String, int>>({});
}
// The following should cause 4 static assertion failures:
//GIVEN( "a map with an invalid type" )
//{
//    struct Invalid {};
//    std::map<String, Invalid> map{ {"key", Invalid{}} };
//    Variant a(map);
//    Variant b(std::move(map));
//    Variant c; c = map;
//    Variant d; d = std::move(map);
//}
}

SCENARIO( "Invalid variant conversion to map", "[Variant]" )
{
GIVEN( "invalid map types" )
{
    struct Foo {};
    checkBadConversionTo<bool>(Variant{true});
    checkBadConversionTo<int>(Variant{Object{ {"key", "Hello"} }});
    // TODO: fix me
    //    checkBadConversionTo<Foo>(Variant{Object{ {"key", 42} }});
    checkBadConversionTo<Null>(Variant{Object{ {"", 0} }});
}
}

SCENARIO( "Comparing variants to maps", "[Variant]" )
{
    using std::map;
    using S = String;

    WHEN( "one side is empty" )
    {
        CHECK( differs(map<S,Null>{},   map<S,Null>{ {"", null} }) );
        CHECK( differs(map<S,Bool>{},   map<S,Bool>{ {"", false} }) );
        CHECK( differs(map<S,Int>{},    map<S,Int>{ {"", 0} }) );
        CHECK( differs(map<S,UInt>{},   map<S,UInt>{ {"", 0u} }) );
        CHECK( differs(map<S,Real>{},   map<S,Real>{ {"", 0.0} }) );
        CHECK( differs(map<S,String>{}, map<S,String>{ {"", ""} }) );
        CHECK( differs(map<S,Array>{},  map<S,Array>{ {"", Array{}} }) );
        CHECK( differs(map<S,Object>{}, map<S,Object>{ {"", Object{}} }) );
    }
    WHEN( "both sides have a single, identical key" )
    {
        CHECK( differs(map<S,Bool>{ {"k", false} }, map<S,Bool>{ {"k", true} }) );
        CHECK( differs(map<S,Int>{ {"k", -1} },     map<S,Int>{ {"k", 0} }) );
        CHECK( differs(map<S,UInt>{ {"k", 0u} },    map<S,UInt>{ {"k", 1u} }) );
        CHECK( differs(map<S,Real>{ {"k", 0.0} },   map<S,Real>{ {"k", 1.0} }) );
        CHECK( differs(map<S,String>{ {"k", "A"} }, map<S,String>{ {"k", "B"} }) );
        CHECK( differs(map<S,Array>{ {"k", Array{}}},
                       map<S,Array>{ {"k", Array{null}}}) );
        CHECK( differs(map<S,Object>{ {"k", Object{}} },
                       map<S,Object>{ {"k", Object{{"",null}}} }) );
    }
    WHEN ( "performing lexicographical comparisons on only the key" )
    {
        using Map = std::map<String, Null>;
        CHECK( differs(Map{ {"A", null} },  Map{ {"AA", null} }) );
        CHECK( differs(Map{ {"A", null} },  Map{ {"B",  null} }) );
        CHECK( differs(Map{ {"A", null} },  Map{ {"a",  null} }) );
        CHECK( differs(Map{ {"B", null} },  Map{ {"BA", null} }) );
        CHECK( differs(Map{ {"B", null} },  Map{ {"a",  null} }) );
    }
    WHEN ( "performing lexicographical comparisons on both key and value" )
    {
        CHECK( differs(map<S,Bool>{ {"A", true} },  map<S,Bool>{ {"AA", false} }) );
        CHECK( differs(map<S,Int>{ {"A", 0} },      map<S,Int>{ {"B",  -1} }) );
        CHECK( differs(map<S,String>{ {"A", "a"} }, map<S,String>{ {"a", "A"} }) );
        CHECK( differs(map<S,Array>{ {"B", Array{null}} },
                       map<S,Array>{ {"BA", Array{}} }) );
        CHECK( differs(map<S,Object>{ {"B", Object{{"",null}}} },
                       map<S,Object>{ {"a", Object{}} }) );
    }
    WHEN ( "elements are of numeric type" )
    {
        CHECK( same   (map<S,Int>{ {"", 0} },    map<S,UInt>{ {"", 0u} }) );
        CHECK( same   (map<S,Int>{ {"", 0} },    map<S,Real>{ {"", 0.0} }) );
        CHECK( same   (map<S,UInt>{ {"", 0u} },  map<S,Real>{ {"", 0.0} }) );
        CHECK( same   (map<S,Int>{ {"", -1} },   map<S,Real>{ {"", -1.0} }) );
        CHECK( differs(map<S,Int>{ {"", 0} },    map<S,UInt>{ {"", 1u} }) );
        CHECK( differs(map<S,Int>{ {"", 0} },    map<S,Real>{ {"", 1.0} }) );
        CHECK( differs(map<S,Int>{ {"", 0} },    map<S,Real>{ {"", 0.1} }) );
        CHECK( differs(map<S,Int>{ {"", -1} },   map<S,Int>{ {"", 0} }) );

        // Signed/unsigned comparison
        CHECK( differs(map<S,UInt>{ {"", 0u} },  map<S,Int>{ {"", -1} }) );

        CHECK( differs(map<S,Int>{ {"", -1} },   map<S,Real>{ {"", 0.0} }) );
        CHECK( differs(map<S,Int>{ {"", -1} },   map<S,Real>{ {"", -0.9} }) );
        CHECK( differs(map<S,UInt>{ {"", 0u} },  map<S,Int>{ {"", 1} }) );
        CHECK( differs(map<S,UInt>{ {"", 0u} },  map<S,Real>{ {"", 1.0} }) );
        CHECK( differs(map<S,UInt>{ {"", 0u} },  map<S,Real>{ {"", 0.1} }) );
        CHECK( differs(map<S,Real>{ {"", 0.0} }, map<S,Int>{ {"", 1} }) );
        CHECK( differs(map<S,Real>{ {"", 0.0} }, map<S,UInt>{ {"", 1u} }) );
    }
}

#endif // #if CPPWAMP_TESTING_VARIANT
