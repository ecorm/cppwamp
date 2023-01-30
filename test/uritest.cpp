/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include <cppwamp/wildcarduri.hpp>
#include <catch2/catch.hpp>
#include <set>
#include <scoped_allocator>
#include <map>
#include <vector>

using namespace wamp;
using namespace wamp::internal;

namespace
{

//------------------------------------------------------------------------------
using Trie = UriTrie<int>;

template <typename T = int>
using TrieTestPair = std::pair<const SplitUri, T>;

template <typename T = int>
using TrieTestPairList = std::vector<TrieTestPair<T>>;

//------------------------------------------------------------------------------
template <typename T>
class UriTrieStatefulAllocator
{
private:
    using Alloc = std::allocator<T>;
    using AllocTraits = std::allocator_traits<Alloc>;

public:
    using value_type = T;
    using pointer = typename AllocTraits::pointer;
    using is_always_equal = std::false_type;
    using propagate_on_container_copy_assignment =
        typename AllocTraits::propagate_on_container_copy_assignment;
    using propagate_on_container_move_assignment =
        typename AllocTraits::propagate_on_container_move_assignment;
    using propagate_on_container_swap =
        typename AllocTraits::propagate_on_container_swap;

    explicit UriTrieStatefulAllocator(int id = 0) noexcept : id_(id) {}

    template <typename U>
    UriTrieStatefulAllocator(const UriTrieStatefulAllocator<U>& rhs) noexcept
        : alloc_(rhs.alloc_),
          id_(rhs.id_)
    {}

    int id() const {return id_;}

    pointer allocate(std::size_t n) {return AllocTraits::allocate(alloc_, n);}

    void deallocate(pointer p, std::size_t n) noexcept
    {
        AllocTraits::deallocate(alloc_, p, n);
    }

    template <typename U, typename... Args>
    void construct(U* p, Args&& ...args)
    {
        AllocTraits::construct(alloc_, p, std::forward<Args>(args)...);
    }

    template <typename U>
    void destroy(U* p) noexcept {AllocTraits::destroy(alloc_, p);}

private:
    Alloc alloc_;
    int id_ = 0;

    template <typename> friend class UriTrieStatefulAllocator;
};

template <class T, class U>
bool operator==(const UriTrieStatefulAllocator<T>& lhs,
                const UriTrieStatefulAllocator<U>& rhs) noexcept
{
    return lhs.id() == rhs.id();
}

template <class T, class U>
bool operator!=(const UriTrieStatefulAllocator<T>& lhs,
                const UriTrieStatefulAllocator<U>& rhs) noexcept
{
    return lhs.id() != rhs.id();
}

//------------------------------------------------------------------------------
template <typename K, typename T, typename C, typename A>
void checkEmptyUriTrie(TokenTrie<K,T,C,A>& t)
{
    const auto& c = t;
    CHECK(c.empty());
    CHECK(c.size() == 0);
    CHECK(c.begin() == c.end());
    CHECK(t.begin() == t.end());
    CHECK(t.cbegin() == t.cend());
}

//------------------------------------------------------------------------------
template <typename TI, typename CI, typename K, typename T>
void checkUriTrieIterators(TI ti, CI ci, const std::pair<const K, T>& pair)
{
    auto key = pair.first;
    auto value = pair.second;
    CHECK( ti.key() == key );
    CHECK( ci.key() == key );
    CHECK( ti->first == key );
    CHECK( ci->first == key );
    CHECK( (*ti).first == key);
    CHECK( (*ci).first == key);
    CHECK( ti.value() == value );
    CHECK( ci.value() == value );
    CHECK( ti->second.get() == value );
    CHECK( ci->second.get() == value );
    CHECK( (*ti).second.get() == value );
    CHECK( (*ci).second.get() == value );

    using Pair = std::pair<const K, T>;
    CHECK( static_cast<Pair>(*ti) == pair );
    CHECK( static_cast<Pair>(*ci) == pair );
}

//------------------------------------------------------------------------------
template <typename TI, typename CI, typename K, typename T>
void checkUriTrieIteratorProxyComparisons(TI ti, CI ci,
                                          const std::pair<const K, T>& pair)
{
    CHECK( *ti == pair );
    CHECK( *ci == pair );
    CHECK( *ti <= pair );
    CHECK( *ci <= pair );
    CHECK( *ti >= pair );
    CHECK( *ci >= pair );
    CHECK_FALSE( *ti != pair );
    CHECK_FALSE( *ci != pair );
    CHECK_FALSE( *ti < pair );
    CHECK_FALSE( *ci < pair );
    CHECK_FALSE( *ti > pair );
    CHECK_FALSE( *ci > pair );
    CHECK( pair == *ti );
    CHECK( pair == *ci );
    CHECK( pair <= *ti );
    CHECK( pair <= *ci );
    CHECK( pair >= *ti );
    CHECK( pair >= *ci );
    CHECK_FALSE( pair != *ti );
    CHECK_FALSE( pair != *ci );
    CHECK_FALSE( pair < *ti );
    CHECK_FALSE( pair < *ci );
    CHECK_FALSE( pair > *ti );
    CHECK_FALSE( pair > *ci );
}

//------------------------------------------------------------------------------
template <typename K, typename T, typename C, typename A>
void checkUriTrieContents(TokenTrie<K,T,C,A>& t,
                          const std::vector<std::pair<const K, T>>& pairs)
{
    if (pairs.empty())
        return checkEmptyUriTrie(t);

    std::map<K, T> m(pairs.begin(), pairs.end());
    const auto& c = t;
    CHECK( c.empty() == m.empty() );
    CHECK( c.size() == m.size() );
    CHECK( c.begin() != c.end() );
    CHECK( t.begin() != t.end() );
    CHECK( t.cbegin() != t.cend() );

    auto ti = t.begin();
    auto ci = c.begin();
    auto mi = m.begin();
    for (unsigned i=0; i<m.size(); ++i)
    {
        auto key = mi->first;
        auto value = mi->second;
        auto pair = *mi;
        INFO( "at position " << i );

        REQUIRE( ti != t.end() );
        REQUIRE( ci != c.end() );
        checkUriTrieIterators(ti, ci, pair);
        checkUriTrieIteratorProxyComparisons(ti, ci, pair);

        CHECK( c.at(key) == value );
        CHECK( t.at(key) == value );
        CHECK( t[key] == value );
        CHECK( t[K{key}] == value );
        CHECK( c.count(key) == 1 );
        CHECK( c.contains(key) );

        auto mf = t.find(key);
        REQUIRE( mf != t.end() );
        CHECK( mf.key() == key );
        CHECK( mf->first == key );
        CHECK( mf.value() == value );
        CHECK( mf->second.get() == value );

        auto cf = c.find(key);
        REQUIRE( cf != t.end() );
        CHECK( cf.key() == key );
        CHECK( cf->first == key );
        CHECK( cf.value() == value );
        CHECK( cf->second.get() == value );

        ++ti;
        ++ci;
        ++mi;
    }

    CHECK( ti == t.end() );
    CHECK( ci == c.end() );
}

//------------------------------------------------------------------------------
using TrieInsertionResult = std::pair<Trie::iterator, bool>;
using TrieInsertionOp =
    std::function<TrieInsertionResult (Trie&, TrieTestPair<>)>;
using TrieInsertionWithUriOp =
    std::function<TrieInsertionResult (Trie&, const std::string&, int)>;

//------------------------------------------------------------------------------
void checkUriTrieInsertion(const TrieTestPairList<>& pairs, bool clobbers,
                           TrieInsertionOp op)
{
    Trie trie;
    for (unsigned i=0; i<pairs.size(); ++i)
    {
        INFO( "for pairs[" << i << "]" );
        const auto& pair = pairs[i];
        auto result = op(trie, pair);
        CHECK(result.second);
        CHECK(result.first.key() == pair.first);
        CHECK(result.first->first == pair.first);
        CHECK(result.first.value() == pair.second);
        CHECK(result.first->second == pair.second);
        CHECK(result.first == trie.find(pair.first));
    }
    checkUriTrieContents(trie, pairs);

    // Check duplicate insertions
    for (unsigned i=0; i<pairs.size(); ++i)
    {
        INFO( "for pairs[" << i << "]" );
        auto pair = pairs[i];
        pair.second = -pair.second;
        auto result = op(trie, pair);
        CHECK_FALSE(result.second);
        CHECK(result.first.key() == pair.first);
        CHECK(result.first->first == pair.first);
        if (!clobbers)
            pair.second = -pair.second;
        CHECK(result.first.value() == pair.second);
        CHECK(result.first->second == pair.second);
    }
}

//------------------------------------------------------------------------------
void checkBadUriTrieAccess(const std::string& info,
                           const TrieTestPairList<>& pairs, const SplitUri& key)
{
    INFO(info);
    SplitUri emptyKey;
    Trie t(pairs.begin(), pairs.end());
    const Trie& c = t;
    CHECK_THROWS_AS(t.at(emptyKey), std::out_of_range);
    CHECK_THROWS_AS(c.at(emptyKey), std::out_of_range);
    CHECK_THROWS_AS(t.at(key), std::out_of_range);
    CHECK_THROWS_AS(c.at(key), std::out_of_range);
    CHECK(t.find(emptyKey) == t.end());
    CHECK(c.find(emptyKey) == c.end());
    CHECK(t.find(key) == t.end());
    CHECK(c.find(key) == c.end());
    CHECK(c.count(emptyKey) == 0);
    CHECK(c.count(key) == 0);
    CHECK_FALSE(c.contains(emptyKey));
    CHECK_FALSE(c.contains(key));
}

//------------------------------------------------------------------------------
bool checkUriTrieUris(const Trie& t, const std::vector<std::string>& uris)
{
    REQUIRE(t.size() == uris.size());
    bool same = true;
    auto iter = t.begin();
    for (unsigned i=0; i<uris.size(); ++i)
    {
        INFO("for uris[" << i << "]");
        CHECK(iter.key() == uris[i]);
        same = same && (iter.key() == uris[i]);
        ++iter;
    }
    return same;
}

//------------------------------------------------------------------------------
bool checkUriTrieIterators(const Trie& t,
                           const std::vector<Trie::iterator>& expected)
{
    bool same = true;
    REQUIRE((t.size() + 1) == expected.size());
    auto iter = t.begin();
    for (unsigned i=0; i<expected.size(); ++i)
    {
        INFO("for expected[" << i << "]");
        CHECK(iter == expected[i]);
        same = same && (iter == expected[i]);
        ++iter;
    }
    return same;
}

//------------------------------------------------------------------------------
void checkUriTrieEqualRange(const Trie& t, const std::string& uri,
                            const std::string& lbUri, const std::string& ubUri)
{
    INFO("For uri '" << uri << "'");

    auto er = t.equal_range(uri);

    auto lb = t.lower_bound(uri);
    CHECK(lb == er.first);
    if (lbUri.empty())
    {
        CHECK(lb == t.end());
    }
    else
    {
        CHECK(lb.key() == lbUri);
        CHECK(er.first.key() == lbUri);
    }

    auto ub = t.upper_bound(uri);
    CHECK(ub == er.second);
    if (ubUri.empty())
    {
        CHECK(ub == t.end());
    }
    else
    {
        CHECK(ub.key() == ubUri);
        CHECK(er.second.key() == ubUri);
    }
}

//------------------------------------------------------------------------------
bool checkUriTrieComparisons(const Trie& a, const Trie& b)
{
    CHECK((a == a));
    CHECK_FALSE((a != a));
    CHECK((b == b));
    CHECK_FALSE((b != b));
    CHECK_FALSE((a == b));
    CHECK((a != b));
    CHECK_FALSE((b == a));
    CHECK((b != a));

    return (a == a) && !(a != a) && (b == b) && !(b != b) &&
           !(a == b) && (a != b) && !(b == a) && (b != a);
}

} // anonymous namespace


//------------------------------------------------------------------------------
TEST_CASE( "URI Tokenization", "[Uri]" )
{
    std::vector<std::pair<std::string, SplitUri::storage_type>> inputs =
    {
        {"",      {""}},
        {"a",     {"a"}},
        {"a.",    {"a", ""}},
        {".",     {"",  ""}},
        {".b",    {"",  "b"}},
        {"a.b",   {"a", "b"}},
        {"..",    {"",  "",  ""}},
        {"..c",   {"",  "",  "c"}},
        {".b.",   {"",  "b", ""}},
        {".b.c",  {"",  "b", "c"}},
        {"a..",   {"a", "",  ""}},
        {"a..c",  {"a", "",  "c"}},
        {"a.b.",  {"a", "b", ""}},
        {"a.b.c", {"a", "b", "c"}},
    };

    for (const auto& pair: inputs)
    {
        const auto& uri = pair.first;
        const auto& labels = pair.second;
        INFO("For URI '" + uri+ "'");
        SplitUri s(labels);
        CHECK(s.labels() == labels);
        CHECK(s.flatten() == uri);
    }
}

//------------------------------------------------------------------------------
TEST_CASE( "URI Wildcard Matching", "[Uri]" )
{
    // Same test vectors as used by Crossbar
    std::vector<std::string> patterns =
    {
         "", ".", "a..c", "a.b.", "a..", ".b.", "..", "x..", ".x.", "..x",
         "x..x", "x.x.", ".x.x", "x.x.x"
    };

    std::vector<std::pair<std::string, std::set<std::string>>> inputs =
    {
        {"abc",     {""}},
        {"a.b",     {"."}},
        {"a.b.c",   {"a..c", "a.b.", "a..", ".b.", ".."}},
        {"a.x.c",   {"a..c", "a..", "..", ".x."}},
        {"a.b.x",   {"a.b.", "a..", ".b.", "..", "..x"}},
        {"a.x.x",   {"a..", "..", ".x.", "..x", ".x.x"}},
        {"x.y.z",   {"..", "x.."}},
        {"a.b.c.d", {}}
    };

    for (const auto& pair: inputs)
    {
        const auto& uri = pair.first;
        const auto& matches = pair.second;
        INFO("For URI '" + uri+ "'");
        for (const auto& pattern: patterns)
        {
            INFO("For pattern '" + pattern+ "'");
            bool uriMatches = matchesWildcardPattern(uri, pattern);
            bool expected = matches.count(pattern) == 1;
            CHECK(uriMatches == expected);
        }
    }
}

//------------------------------------------------------------------------------
TEST_CASE( "Empty UriTrie Construction", "[Uri]" )
{
    Trie empty;

    SECTION( "default contruction" )
    {
        checkEmptyUriTrie(empty);
    };

    SECTION( "via iterator range" )
    {
        std::map<SplitUri, int> m;
        Trie trie(m.begin(), m.end());
        checkEmptyUriTrie(trie);
    };

    SECTION( "via initializer list" )
    {
        Trie trie({});
        checkEmptyUriTrie(trie);
    };

    SECTION( "via copy constructor" )
    {
        Trie b(empty);
        checkEmptyUriTrie(empty);
        checkEmptyUriTrie(b);
    };

    SECTION( "via move constructor" )
    {
        Trie b(std::move(empty));
        checkEmptyUriTrie(empty);
        checkEmptyUriTrie(b);
    };

    SECTION( "via copy assignment" )
    {
        Trie b{{{"a"}, 1}};
        b = empty;
        checkEmptyUriTrie(empty);
        checkEmptyUriTrie(b);
    };

    SECTION( "via move assignment" )
    {
        Trie b{{{"a"}, 1}};
        b = std::move(empty);
        checkEmptyUriTrie(empty);
        checkEmptyUriTrie(b);
    };
}

//------------------------------------------------------------------------------
TEST_CASE( "UriTrie Insertion", "[Uri]" )
{
    using Pair = TrieTestPair<>;

    std::vector<TrieTestPairList<>> inputs =
    {
        { {"",      1} },
        { {"a",     1} },
        { {"a.b",   1} },
        { {"a.b.c", 1} },
        { {"a",     1},  {"b",     2}},
        { {"b",     1},  {"a",     2}},
        { {"a",     1},  {"a.b",   2}},
        { {"a",     1},  {"a.b.c", 2}},
        { {"a.b",   1},  {"a",     2}},
        { {"a.b",   1},  {"b",     2}},
        { {"a.b",   1},  {"b.a",   2}},
        { {"a.b",   1},  {"c.d",   2}},
        { {"a.b.c", 1},  {"a",     2}},
        { {"a.b.c", 1},  {"b",     2}},
        { {"a.b.c", 1},  {"c",     2}},
        { {"a.b.c", 1},  {"d",     2}},
        { {"a.b.c", 1},  {"a.b",   2}},
        { {"a.b.c", 1},  {"b.c",   2}},
        { {"a.b.c", 1},  {"d.e",   2}},
        { {"a.b.c", 1},  {"a.b.d", 2}},
        { {"a.b.c", 1},  {"a.d.e", 2}},
        { {"a.b.c", 1},  {"d.e.f", 2}},
        { {"d",     4},  {"a", 1}, {"c", 3}, {"b", 2}, {"e", 5}},
    };

    for (unsigned i=0; i<inputs.size(); ++i)
    {
        const auto& input = inputs[i];
        INFO( "for inputs[" << i << "]" );

        SECTION( "via constuctor taking iterator range" )
        {
            Trie trie(input.begin(), input.end());
            checkUriTrieContents(trie, input);
        };

        SECTION( "via constuctor taking special iterator range" )
        {
            Trie a(input.begin(), input.end());
            Trie b(a.begin(), a.end());
            checkUriTrieContents(b, input);
        };

        SECTION( "via insert iterator range" )
        {
            Trie trie;
            trie.insert(input.begin(), input.end());
            checkUriTrieContents(trie, input);
        };

        SECTION( "via insert special iterator range" )
        {
            Trie a(input.begin(), input.end());
            Trie b;
            b.insert(a.begin(), a.end());
            checkUriTrieContents(b, input);
        };

        SECTION( "via insert pair" )
        {
            checkUriTrieInsertion(input, false,
                [](Trie& t, Pair p) {return t.insert(p);});
        };

        SECTION( "via insert moved pair" )
        {
            checkUriTrieInsertion(input, false,
                [](Trie& t, Pair p) {return t.insert(Pair{p});});
        };

        SECTION( "via insert_or_assign" )
        {
            checkUriTrieInsertion(input, true,
                [](Trie& t, Pair p)
                {
                    return t.insert_or_assign(p.first, p.second);
                });
        };

        SECTION( "via insert_or_assign with moved key" )
        {
            checkUriTrieInsertion(input, true,
                [](Trie& t, Pair p)
                {
                    return t.insert_or_assign(std::move(p.first), p.second);
                });
        };

        SECTION( "via emplace" )
        {
            checkUriTrieInsertion(input, false,
                [](Trie& t, Pair p) {return t.emplace(p.first, p.second);});
        };

        SECTION( "via try_emplace" )
        {
            checkUriTrieInsertion(input, false,
                [](Trie& t, Pair p) {return t.try_emplace(p.first, p.second);});
        };

        SECTION( "via try_emplace with moved key" )
        {
            checkUriTrieInsertion(input, false,
                [](Trie& t, Pair p)
                {
                    return t.try_emplace(std::move(p.first), p.second);
                });
        };

        SECTION( "via operator[]" )
        {
            checkUriTrieInsertion(input, true,
                [](Trie& t, Pair p)
                {
                    bool inserted = t.find(p.first) == t.end();
                    t[p.first] = p.second;
                    return std::make_pair(t.find(p.first), inserted);
                });
        };

        SECTION( "via operator[] with moved key" )
        {
            checkUriTrieInsertion(input, true,
                [](Trie& t, Pair p)
                {
                    bool inserted = t.find(p.first) == t.end();
                    t[std::move(p.first)] = p.second;
                    return std::make_pair(t.find(p.first), inserted);
                });
        };
    }
}

//------------------------------------------------------------------------------
TEST_CASE( "UriTrie Inializer Lists", "[Uri]" )
{
    TrieTestPairList<> pairs({ {"a.b.c", 1}, {"a", 2} });

    SECTION( "constructor taking initializer list" )
    {
        Trie trie({ {"a.b.c", 1}, {"a", 2} });
        checkUriTrieContents(trie, pairs);
    };

    SECTION( "assignment from initializer list" )
    {
        Trie trie({ {"z", 3} });
        trie = { {"a.b.c", 1}, {"a", 2} };
        checkUriTrieContents(trie, pairs);
    };

    SECTION( "assignment from empty initializer list" )
    {
        Trie trie({ {{"z"}, 3} });
        trie = {};
        checkEmptyUriTrie(trie);
    };
}

//------------------------------------------------------------------------------
TEST_CASE( "UriTrie Copy/Move Construction/Assignment", "[Uri]" )
{
    std::vector<TrieTestPairList<>> inputs = {
        { },
        { {"a", 1} },
        { {"a.b.c", 1}, {"a.b", 2}},
        { {"a.b.c", 1}, {"d.e", 2}},
    };

    for (unsigned i=0; i<inputs.size(); ++i)
    {
        INFO("for input[" << i << "]");
        const auto& input = inputs[i];
        Trie a(input.begin(), input.end());
        auto aBegin = a.begin();
        auto aEnd = a.end();

        SECTION( "copy construction" )
        {
            Trie b(a);
            checkUriTrieContents(a, input);
            checkUriTrieContents(b, input);

            // Check iterators to RHS are preserved
            CHECK(aEnd == a.end());
            CHECK(aBegin == a.begin());
            if (!input.empty())
            {
                REQUIRE(aBegin != aEnd);
                CHECK(aBegin.key() == input.front().first);
            }
            if (input.size() == 1)
                CHECK(++aBegin == aEnd);
        };

        SECTION( "move construction" )
        {
            Trie b(std::move(a));
            checkEmptyUriTrie(a);
            checkUriTrieContents(b, input);

            // Check non-end iterators to RHS are preserved
            if (!input.empty())
            {
                REQUIRE(b.begin() != b.end());
                CHECK(aBegin == b.begin());
                CHECK(b.begin().key() == input.front().first);
                if (input.size() == 1)
                    CHECK(++aBegin == b.end());
            }
        };

        SECTION( "copy assignment to empty trie" )
        {
            Trie b;
            b = a;
            checkUriTrieContents(a, input);
            checkUriTrieContents(b, input);

            // Check iterators to RHS are preserved
            CHECK(aEnd == a.end());
            CHECK(aBegin == a.begin());
            if (!input.empty())
            {
                REQUIRE(aBegin != aEnd);
                CHECK(aBegin.key() == input.front().first);
            }
            if (input.size() == 1)
                CHECK(++aBegin == aEnd);
        };

        SECTION( "copy assignment to non-empty trie" )
        {
            Trie b({ {{"x"}, 3} });
            b = a;
            checkUriTrieContents(a, input);
            checkUriTrieContents(b, input);

            // Check iterators to RHS are preserved
            CHECK(aEnd == a.end());
            CHECK(aBegin == a.begin());
            if (!input.empty())
            {
                REQUIRE(aBegin != aEnd);
                CHECK(aBegin.key() == input.front().first);
            }
            if (input.size() == 1)
                CHECK(++aBegin == aEnd);
        };

        SECTION( "move assignment to empty trie" )
        {
            Trie b;
            b = std::move(a);
            checkEmptyUriTrie(a);
            checkUriTrieContents(b, input);

            // Check non-end iterators to RHS are preserved
            if (!input.empty())
            {
                REQUIRE(b.begin() != b.end());
                CHECK(aBegin == b.begin());
                CHECK(b.begin().key() == input.front().first);
            }
            if (input.size() == 1)
                CHECK(++aBegin == b.end());
        };

        SECTION( "move assignment to non-empty trie" )
        {
            Trie b({ {{"x"}, 3} });
            b = std::move(a);
            checkEmptyUriTrie(a);
            checkUriTrieContents(b, input);

            // Check non-end iterators to RHS are preserved
            if (!input.empty())
            {
                REQUIRE(b.begin() != b.end());
                CHECK(aBegin == b.begin());
                CHECK(b.begin().key() == input.front().first);
            }
            if (input.size() == 1)
                CHECK(++aBegin == b.end());
        };
    }
}

//------------------------------------------------------------------------------
TEST_CASE( "UriTrie Self-Assignment", "[Uri]" )
{
    SECTION( "self copy assignment of populated trie" )
    {
        Trie t({ {{"a"}, 1} });
        auto& r = t; // To avoid self-assignment warnings
        auto begin = t.begin();
        auto end = t.end();
        t = r;
        CHECK(t.size() == 1);
        REQUIRE(t.contains("a"));
        CHECK(t["a"] == 1);
        CHECK(begin == t.begin());
        CHECK(end == t.end());
        CHECK(begin.key() == "a");
        CHECK(begin.value() == 1);
        CHECK(++begin == end);
    }

    SECTION( "self copy assignment of empty trie" )
    {
        Trie t;
        auto& r = t;
        auto end = t.end();
        t = r;
        CHECK(t.empty());
        CHECK(end == t.begin());
        CHECK(end == t.end());
    }

    SECTION( "self move assignment of populated trie" )
    {
        Trie t({ {{"a"}, 1} });
        auto& r = t;
        auto begin = t.begin();
        auto end = t.end();
        t = std::move(r);
        CHECK(t.size() == 1);
        REQUIRE(t.contains("a"));
        CHECK(t["a"] == 1);
        CHECK(begin == t.begin());
        CHECK(end == t.end());
        CHECK(begin.key() == "a");
        CHECK(begin.value() == 1);
        CHECK(++begin == end);
    }

    SECTION( "self copy assignment of empty trie" )
    {
        Trie t;
        auto& r = t;
        auto end = t.end();
        t = std::move(r);
        CHECK(t.empty());
        CHECK(end == t.begin());
        CHECK(end == t.end());
    }
}

//------------------------------------------------------------------------------
TEST_CASE( "Reusing Moved UriTrie", "[Uri]" )
{
    TrieTestPairList<> pairs({ {"a.b.c", 1}, {"a", 2} });
    Trie a({ {{"d"}, 3} });

    SECTION( "move constructor" )
    {
        Trie b(std::move(a));
        checkEmptyUriTrie(a);
        a.insert(pairs.begin(), pairs.end());
        checkUriTrieContents(a, pairs);
    }

    SECTION( "move assignment" )
    {
        Trie b;
        b = (std::move(a));
        checkEmptyUriTrie(a);
        a.insert(pairs.begin(), pairs.end());
        checkUriTrieContents(a, pairs);
    }
}

//------------------------------------------------------------------------------
TEST_CASE( "UriTrie Bad Access/Lookups", "[Uri]" )
{
    auto check = [](const std::string& info, const TrieTestPairList<>& pairs,
                    const SplitUri& key)
    {
        checkBadUriTrieAccess(info, pairs, key);
    };

    check("empty trie",        {},           "a");
    check("populated trie",    {{"a",   1}}, "b");
    check("trie has wildcard", {{"",    1}}, "a");
    check("key is wildcard",   {{"a",   1}}, "");
    check("key is prefix",     {{"a.b", 1}}, "a");
    check("key is partial",    {{"a.b", 1}}, "a.c");
    check("key too long",      {{"a",   1}}, "a.b");
}

//------------------------------------------------------------------------------
TEST_CASE( "UriTrie Lower/Upper Bound and Equal Range", "[Uri]" )
{
    SECTION ("Empty trie")
    {
        Trie t;
        auto end = t.end();
        CHECK(t.lower_bound("") == end);
        CHECK(t.lower_bound(" ") == end);
        CHECK(t.lower_bound("a") == end);
        CHECK(t.lower_bound("a.b") == end);
        CHECK(t.lower_bound(SplitUri{}) == end);
        CHECK(t.upper_bound("") == end);
        CHECK(t.upper_bound(" ") == end);
        CHECK(t.upper_bound("a") == end);
        CHECK(t.upper_bound("a.b") == end);
        CHECK(t.upper_bound(SplitUri{}) == end);
    }

    SECTION ("Populated trie")
    {
        Trie t({{"a", 1}, {"a.b.c", 2}, {"d", 3}, {"d.f", 4}});

        auto check = [&t](const std::string& uri, const std::string& lbUri,
                          const std::string& ubUri)
        {
            return checkUriTrieEqualRange(t, uri, lbUri, ubUri);
        };

        auto end = t.end();
        check("",        "a",       "a");
        check(" ",       "a",       "a");
        check("`",       "a",       "a");
        check("a",       "a",       "a.b.c");
        check("a.",      "a.b.c",   "a.b.c");
        check("a.b",     "a.b.c",   "a.b.c");
        check("a.b.",    "a.b.c",   "a.b.c");
        check("a.b. ",   "a.b.c",   "a.b.c");
        check("a.b.a",   "a.b.c",   "a.b.c");
        check("a.b.c",   "a.b.c",   "d");
        check("a ",      "d",       "d");
        check("aa",      "d",       "d");
        check("a.b ",    "d",       "d");
        check("a.ba",    "d",       "d");
        check("a.b.c ",  "d",       "d");
        check("a.b.c.",  "d",       "d");
        check("a.b.c.d", "d",       "d");
        check("a.b.d",   "d",       "d");
        check("a.c",     "d",       "d");
        check("b",       "d",       "d");
        check("b.c",     "d",       "d");
        check("c",       "d",       "d");
        check("d",       "d",       "d.f");
        check("d.",      "d.f",     "d.f");
        check("d.e",     "d.f",     "d.f");
        check("d.e ",    "d.f",     "d.f");
        check("d.f",     "d.f",     "");
        check("d.f ",    "",        "");
        check("d.g",     "",        "");
        check("d ",      "",        "");
        check("da",      "",        "");
        check("e",       "",        "");

        CHECK(t.lower_bound(SplitUri{}) == end);
        CHECK(t.upper_bound(SplitUri{}) == end);
        auto er = t.equal_range(SplitUri{});
        CHECK(er.first == end);
        CHECK(er.second == end);
    }
}

//------------------------------------------------------------------------------
TEST_CASE( "UriTrie Pattern Matching", "[Uri]" )
{
    // Same test vectors as used by Crossbar
    std::vector<std::string> patterns =
    {
        "", ".", "a..c", "a.b.", "a..", ".b.", "..", "x..", ".x.", "..x",
        "x..x", "x.x.", ".x.x", "x.x.x"
    };

    std::vector<std::pair<std::string, std::set<std::string>>> inputs =
    {
        {"abc",     {""}},
        {"a.b",     {"."}},
        {"a.b.c",   {"a..c", "a.b.", "a..", ".b.", ".."}},
        {"a.x.c",   {"a..c", "a..", "..", ".x."}},
        {"a.b.x",   {"a.b.", "a..", ".b.", "..", "..x"}},
        {"a.x.x",   {"a..", "..", ".x.", "..x", ".x.x"}},
        {"x.y.z",   {"..", "x.."}},
        {"a.b.c.d", {}},

        // Additional corner cases where looked-up URIs have empty labels
        {"",        {""}},
        {".",       {"."}},
        {".b",      {"."}},
        {"a.",      {"."}},
        {"..c",     {".."}},
        {".b.",     {".b.", ".."}},
        {".b.c",    {".b.", ".."}},
        {"a..",     {"a..", ".."}},
        {"a..c",    {"a..c", "a..", ".."}},
        {"a.b.",    {"a.b.", "a..", ".b.", ".."}},
        {".x.",     {"..", ".x."}},
        {".x.c",    {"..", ".x."}},
        {"a.x.",    {"a..", "..", ".x."}},
        {"..x",     {"..", "..x"}},
        {".b.x",    {".b.", "..", "..x"}},
        {"a..x",    {"a..", "..", "..x"}},
        {".x.x",    {"..", ".x.", "..x", ".x.x"}},
        {"..z",     {".."}},
        {".y.",     {".."}},
        {".y.z",    {".."}},
        {"x..",     {"..", "x.."}},
        {"x.y.z",   {"..", "x.."}},
        {"x..z",    {"..", "x.."}},
        {"x.y.",    {"..", "x.."}},
        {"...",     {}},
        {"a...",    {}},
        {"a.b..",   {}},
        {".b..",    {}},
        {"a..c.",   {}},
        {"a.b.c.d", {}},
        {"a.b.c.",  {}},
        {"a.b..d",  {}},
        {"a..c.d",  {}},
        {".b.c.d",  {}},
    };

    UriTrie<std::string> trie;
    for (const auto& pattern: patterns)
        trie.insert_or_assign(pattern, pattern);

    for (unsigned i=0; i<inputs.size(); ++i)
    {
        INFO( "for input[" << i << "]" );
        auto uri = inputs[i].first;
        auto key = SplitUri(uri);
        auto expectedHits = inputs[i].second;

        auto matches = wildcardMatches(trie, key);
        std::set<std::string> hits;
        for (unsigned i = 0; i != expectedHits.size(); ++i)
        {
            REQUIRE_FALSE(matches.done());
            auto matchKey = matches.key();
            auto matchUri = matchKey.flatten().value();
            CHECK( matchKey == matchUri );
            CHECK( matches.value() == matchUri );
            REQUIRE( hits.emplace(matchUri).second );
            matches.next();
        }
        CHECK(matches.done());
        CHECK(hits == expectedHits);
    }
}

//------------------------------------------------------------------------------
TEST_CASE( "UriTrie Erase", "[Uri]" )
{
    Trie trie({ {"a", 1}, {"a.b.c", 2}, {"d", 3}, {"d.e", 4} });

    SECTION( "erasing via iterator" )
    {
        auto pos = trie.find("a.b.c");
        REQUIRE(pos != trie.end());
        auto iter = trie.erase(pos);
        CHECK(iter == trie.find("d"));
        CHECK(checkUriTrieUris(trie, {"a", "d", "d.e"}));
        // Check pruning below "a" node
        CHECK(trie.find("a").cursor().child()->children().empty());

        pos = trie.find("d");
        REQUIRE(pos != trie.end());
        iter = trie.erase(pos);
        CHECK(iter == trie.find("d.e"));
        CHECK(checkUriTrieUris(trie, {"a", "d.e"}));
        // Check non-value "d" node still exists
        auto rootNode = trie.begin().cursor().parent();
        CHECK(rootNode->children().find("d") != rootNode->children().end());
        CHECK(!rootNode->children().find("d")->second.has_value());

        pos = trie.find("a");
        REQUIRE(pos != trie.end());
        iter = trie.erase(pos);
        CHECK(iter == trie.find("d.e"));
        CHECK(checkUriTrieUris(trie, {"d.e"}));
        // Check root node has a single non-value "d" child node
        REQUIRE(rootNode->children().size() == 1);
        CHECK(rootNode->children().find("d") != rootNode->children().end());
        CHECK(!rootNode->children().find("d")->second.has_value());

        // Re-insert last deleted key and erase it again
        auto inserted = trie.try_emplace("a", 1);
        REQUIRE(inserted.second);
        CHECK(checkUriTrieUris(trie, {"a", "d.e"}));
        iter = trie.erase(inserted.first);
        CHECK(iter == trie.find("d.e"));
        CHECK(checkUriTrieUris(trie, {"d.e"}));
        // Check root node has a single non-value "d" child node
        REQUIRE(rootNode->children().size() == 1);
        CHECK(rootNode->children().find("d") != rootNode->children().end());
        CHECK(!rootNode->children().find("d")->second.has_value());

        pos = trie.find("d.e");
        REQUIRE(pos != trie.end());
        iter = trie.erase(pos);
        CHECK(iter == trie.end());
        CHECK(trie.empty());
        // Check root node has no child nodes
        CHECK(rootNode->children().empty());
    }

    SECTION( "erasing via key" )
    {
        CHECK_FALSE(trie.erase("z"));
        CHECK(checkUriTrieUris(trie, {"a", "a.b.c", "d", "d.e"}));

        CHECK(trie.erase("a.b.c"));
        CHECK(checkUriTrieUris(trie, {"a", "d", "d.e"}));

        CHECK(trie.erase("d"));
        CHECK(checkUriTrieUris(trie, {"a", "d.e"}));

        CHECK(trie.erase("a"));
        CHECK(checkUriTrieUris(trie, {"d.e"}));

        // Re-insert last deleted key and erase it again
        auto inserted = trie.try_emplace("a", 1);
        REQUIRE(inserted.second);
        CHECK(trie.erase("a"));
        CHECK(checkUriTrieUris(trie, {"d.e"}));

        CHECK(trie.erase("d.e"));
        CHECK(trie.empty());
    }
}

//------------------------------------------------------------------------------
TEST_CASE( "UriTrie Clear", "[Uri]" )
{
    SECTION("non-empty trie")
    {
        Trie t({ {{"a"}, 1} });
        t.clear();
        checkEmptyUriTrie(t);
        t.clear();
        checkEmptyUriTrie(t);
    }

    SECTION("default-constructed trie")
    {
        Trie t;
        t.clear();
        checkEmptyUriTrie(t);
        t.clear();
        checkEmptyUriTrie(t);
    }
}

//------------------------------------------------------------------------------
TEST_CASE( "UriTrie Swap", "[Uri]" )
{
    Trie a({ {{"a"}, 1} });
    auto aBegin = a.begin();
    Trie b({ {{"b"}, 2}, {{"c"}, 3} });
    auto bBegin = b.begin();
    Trie x;
    Trie y;

    SECTION("populated tries")
    {
        a.swap(b);
        CHECK(a.size() == 2);
        CHECK(a.contains("b"));
        CHECK(a.contains("c"));
        CHECK(aBegin == b.begin());
        REQUIRE(aBegin != b.end());
        CHECK(aBegin.key() == "a");
        CHECK(++Trie::iterator(aBegin) == b.end());
        CHECK(b.size() == 1);
        CHECK(b.contains("a"));
        CHECK(bBegin == a.begin());
        REQUIRE(bBegin != a.end());
        CHECK(bBegin.key() == "b");
        CHECK((++Trie::iterator(bBegin)).key() == "c");
        CHECK(++(++Trie::iterator(bBegin)) == a.end());

        swap(b, a);
        CHECK(a.size() == 1);
        CHECK(a.contains("a"));
        CHECK(aBegin == a.begin());
        REQUIRE(aBegin != a.end());
        CHECK(aBegin.key() == "a");
        CHECK(++Trie::iterator(aBegin) == a.end());
        CHECK(b.size() == 2);
        CHECK(b.contains("b"));
        CHECK(b.contains("c"));
        CHECK(bBegin == b.begin());
        REQUIRE(bBegin != b.end());
        CHECK(bBegin.key() == "b");
        CHECK((++Trie::iterator(bBegin)).key() == "c");
        CHECK(++(++Trie::iterator(bBegin)) == b.end());
    }

    SECTION("rhs trie is empty")
    {
        a.swap(x);
        CHECK(a.empty());
        CHECK(aBegin == x.begin());
        REQUIRE(aBegin != x.end());
        CHECK(aBegin.key() == "a");
        CHECK(++Trie::iterator(aBegin) == x.end());
        CHECK(x.size() == 1);
        CHECK(x.contains("a"));

        swap(x, a);
        CHECK(a.size() == 1);
        CHECK(a.contains("a"));
        CHECK(aBegin == a.begin());
        CHECK(++Trie::iterator(aBegin) == a.end());
        REQUIRE(aBegin != a.end());
        CHECK(aBegin.key() == "a");
        CHECK(x.empty());
    }

    SECTION("lhs trie is empty")
    {
        x.swap(a);
        CHECK(x.size() == 1);
        CHECK(x.contains("a"));
        CHECK(a.empty());
        CHECK(aBegin == x.begin());
        REQUIRE(aBegin != x.end());
        CHECK(aBegin.key() == "a");
        CHECK(++Trie::iterator(aBegin) == x.end());

        swap(a, x);
        CHECK(a.size() == 1);
        CHECK(a.contains("a"));
        CHECK(aBegin == a.begin());
        REQUIRE(aBegin != a.end());
        CHECK(aBegin.key() == "a");
        CHECK(++Trie::iterator(aBegin) == a.end());
        CHECK(x.empty());
    }

    SECTION("both tries are empty")
    {
        x.swap(y);
        CHECK(x.empty());
        CHECK(y.empty());

        swap(y, x);
        CHECK(x.empty());
        CHECK(y.empty());
    }

    SECTION("self-swap populated trie")
    {
        a.swap(a);
        CHECK(a.size() == 1);
        CHECK(a.contains("a"));
        CHECK(aBegin == a.begin());
        REQUIRE(aBegin != a.end());
        CHECK(aBegin.key() == "a");
        CHECK(++Trie::iterator(aBegin) == a.end());

        swap(b, b);
        CHECK(b.size() == 2);
        CHECK(b.contains("b"));
        CHECK(b.contains("c"));
        CHECK(bBegin == b.begin());
        REQUIRE(bBegin != b.end());
        CHECK(bBegin.key() == "b");
        CHECK((++Trie::iterator(bBegin)).key() == "c");
        CHECK(++(++Trie::iterator(bBegin)) == b.end());
    }

    SECTION("self-swap empty trie")
    {
        x.swap(x);
        CHECK(x.empty());

        swap(y, y);
        CHECK(y.empty());
    }
}

//------------------------------------------------------------------------------
TEST_CASE( "UriTrie Modification Preserves Iterators", "[Uri]" )
{
    Trie t;
    auto z = t.end();
    auto b = t.insert_or_assign("b", 2).first;
    CHECK(checkUriTrieIterators(t, {b, z}));
    auto a = t.insert_or_assign("a", 2).first;
    CHECK(checkUriTrieIterators(t, {a, b, z}));
    auto d = t.insert_or_assign("d", 4).first;
    CHECK(checkUriTrieIterators(t, {a, b, d, z}));
    auto bc = t.insert_or_assign("b.c", 3).first;
    CHECK(checkUriTrieIterators(t, {a, b, bc, d, z}));
    t.erase("b");
    CHECK(checkUriTrieIterators(t, {a, bc, d, z}));
    t.erase("a");
    CHECK(checkUriTrieIterators(t, {bc, d, z}));
    t.erase("d");
    CHECK(checkUriTrieIterators(t, {bc, z}));
    t.erase("b.c");
    CHECK(checkUriTrieIterators(t, {z}));
}

//------------------------------------------------------------------------------
TEST_CASE( "UriTrie Comparisons", "[Uri]" )
{
    auto check = [](const Trie& a, const Trie& b) -> bool
    {
        return checkUriTrieComparisons(a, b);
    };

    CHECK( check({{}},                      {{"a", 1}}) );
    CHECK( check({{"a",   1}},              {{"a", 2}}) );
    CHECK( check({{"a",   1}},              {{"b", 1}}) );
    CHECK( check({{"a.b", 1}},              {{"a", 1}}) );
    CHECK( check({{"a",   1}, {"b",   2}},  {{"a", 1}}) );
    CHECK( check({{"a",   1}, {"a.b", 2}},  {{"a.b", 2}}) );
}

//------------------------------------------------------------------------------
TEST_CASE( "UriTrie erase_if", "[Uri]" )
{
    Trie trie({{"a", 1}, {"b", 2}, {"b.c", 1}});

    SECTION( "criteria based on value" )
    {
        auto n = erase_if(
            trie,
            [](Trie::value_type kv) {return kv.second == 1;} );
        CHECK(n == 2);
        checkUriTrieUris(trie, {"b"});
    }

    SECTION( "criteria based on key" )
    {
        auto n = erase_if(
            trie,
            [](Trie::value_type kv) {return kv.first.front() == "b";} );
        CHECK(n == 2);
        checkUriTrieUris(trie, {"a"});
    }
}

//------------------------------------------------------------------------------
TEST_CASE( "UriTrie Iterator Conversions and Mixed Comparisons", "[Uri]" )
{
    using CI = Trie::const_iterator;
    using MI = Trie::iterator;

    CHECK(std::is_convertible<CI, MI>::value == false);
    CHECK(std::is_convertible<MI, CI>::value == true);

    Trie t({ {{"a"}, 1} });
    CI ci = t.cbegin();
    MI mi = t.begin();

    CHECK(CI(ci).key() == "a");
    CHECK(CI(mi).key() == "a");
    CHECK(MI(mi).key() == "a");

    CHECK((ci == ci));
    CHECK((ci == mi));
    CHECK((mi == ci));
    CHECK((mi == mi));

    CHECK_FALSE((ci != ci));
    CHECK_FALSE((ci != mi));
    CHECK_FALSE((mi != ci));
    CHECK_FALSE((mi != mi));
}

//------------------------------------------------------------------------------
template <typename K, typename T, typename C, typename A>
void checkTrieStatefulAllocator(const TokenTrie<K,T,C,A>& trie, int id)
{
    auto cursor = trie.root();
    unsigned pos = 0;
    while (cursor != trie.sentinel())
    {
        INFO( "At cursor position " << pos <<
             " with token " << cursor.token());
        CHECK( cursor.parent()->children().get_allocator().id() == id );
        CHECK( cursor.token().get_allocator().id() == id );
        if (cursor.has_value())
            CHECK( cursor.value().get_allocator().id() == id );
        cursor.advance_depth_first_to_next_node();
        ++pos;
    }
}

//------------------------------------------------------------------------------
TEST_CASE( "UriTrie with scoped_allocator_adapter", "[Uri]" )
{
    SECTION("with std::allocator<int>")
    {
        using A = std::scoped_allocator_adaptor<std::allocator<int>>;
        TrieTestPairList<> pairs({ {"a.b.c", 1}, {"a", 2} });

        UriTrie<int, A> trie({ {"a.b.c", 1}, {"a", 2} });
        checkUriTrieContents(trie, pairs);
    }

    SECTION("with stateful allocator")
    {
        using A = std::scoped_allocator_adaptor<UriTrieStatefulAllocator<char>>;
        using StringType = std::basic_string<char, std::char_traits<char>,
                                             UriTrieStatefulAllocator<char>>;
        using Key = std::vector<StringType, A>;
        using Value = StringType;
        using TrieType = TokenTrie<Key, Value, TokenTrieDefaultOrdering, A>;
        using Pair = std::pair<const Key, Value>;

        A alloc1(101);
        A alloc2(102);
        std::vector<Pair> pairs({ {{"a", "b", "c"}, "foo"}, {{"a"}, "bar"} });
        TrieType trie1(pairs.begin(), pairs.end(), alloc1);

        SECTION("construction with allocator")
        {
            CHECK(trie1.size() == pairs.size());
            checkUriTrieContents(trie1, pairs);
            checkTrieStatefulAllocator(trie1, alloc1.id());
        }

        SECTION("copy construction propagates allocator")
        {
            TrieType trie2(trie1);
            CHECK(trie2.size() == pairs.size());
            checkTrieStatefulAllocator(trie2, alloc1.id());
        }

        SECTION("move construction propagates allocator")
        {
            TrieType trie2(std::move(trie1));
            CHECK(trie2.size() == pairs.size());
            checkTrieStatefulAllocator(trie2, alloc1.id());
        }

        SECTION("copy assignment does not propagate allocator")
        {
            TrieType trie2(alloc2);
            trie2 = trie1;
            CHECK(trie2.size() == pairs.size());
            checkTrieStatefulAllocator(trie2, alloc2.id());
        }

        SECTION("move assignment propagates allocator")
        {
            TrieType trie2(alloc2);
            trie2 = std::move(trie1);
            CHECK(trie2.size() == pairs.size());
            checkTrieStatefulAllocator(trie2, alloc1.id());
        }

        SECTION("swap does not propagate allocators")
        {
            std::vector<Pair> pairs2({{{"d"}, "baz"}});
            TrieType trie2(pairs2.begin(), pairs2.end(), alloc2);
            trie2.swap(trie1);
            CHECK(trie1.size() == pairs2.size());
            CHECK(trie2.size() == pairs.size());
            SECTION("trie1 preserves original allocator")
            {
                checkTrieStatefulAllocator(trie1, alloc1.id());
            }
            SECTION("trie2 preserves original allocator")
            {
                checkTrieStatefulAllocator(trie2, alloc2.id());
            }
        }
    }
}
