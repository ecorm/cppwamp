/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include <cppwamp/internal/trie.hpp>
#include <catch2/catch.hpp>
#include <set>
#include <map>
#include <vector>

using namespace wamp;
using namespace wamp::internal;

namespace
{

//------------------------------------------------------------------------------
using Trie = WildcardTrie<int>;
using TrieTestPair = std::pair<const SplitUri, int>;
using TrieTestPairList = std::vector<TrieTestPair>;

//------------------------------------------------------------------------------
template <typename T>
void checkEmptyWildcardTrie(WildcardTrie<T>& t)
{
    const WildcardTrie<T>& c = t;
    CHECK(c.empty());
    CHECK(c.size() == 0);
    CHECK(c.begin() == c.end());
    CHECK(t.begin() == t.end());
    CHECK(t.cbegin() == t.cend());
}

//------------------------------------------------------------------------------
template <typename T>
void checkWildcardTrieContents(WildcardTrie<T>& t,
                               const TrieTestPairList& pairs)
{
    if (pairs.empty())
        return checkEmptyWildcardTrie(t);

    std::map<SplitUri, T> m(pairs.begin(), pairs.end());
    const WildcardTrie<T>& c = t;
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
        auto uri = untokenizeUri(key);
        auto value = mi->second;
        INFO( "at position " << i );

        REQUIRE( ti != t.end() );
        REQUIRE( ci != c.end() );

        CHECK( *ti == value );
        CHECK( *ci == value );
        CHECK( ti.value() == value );
        CHECK( ci.value() == value );
        CHECK( ti.key() == key );
        CHECK( ci.key() == key );
        CHECK( c.at(key) == value );
        CHECK( t.at(key) == value );
        CHECK( c.at(uri) == value );
        CHECK( t.at(uri) == value );
        CHECK( t[key] == value );
        CHECK( t[SplitUri{key}] == value );
        CHECK( t[uri] == value );
        CHECK( c.count(key) == 1 );
        CHECK( c.count(uri) == 1 );
        CHECK( c.contains(key) );
        CHECK( c.contains(uri) );

        auto mf = t.find(key);
        REQUIRE( mf != t.end() );
        CHECK( *mf == value );
        CHECK( mf.key() == key );
        CHECK( mf.value() == value );

        mf = t.find(uri);
        REQUIRE( mf != t.end() );
        CHECK( *mf == value );
        CHECK( mf.key() == key );
        CHECK( mf.uri() == uri );
        CHECK( mf.value() == value );

        auto cf = c.find(key);
        REQUIRE( cf != c.end() );
        CHECK( *cf == value );
        CHECK( cf.key() == key );
        CHECK( cf.uri() == uri );
        CHECK( cf.value() == value );

        cf = c.find(uri);
        REQUIRE( cf != c.end() );
        CHECK( *cf == value );
        CHECK( cf.key() == key );
        CHECK( cf.value() == value );

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
    std::function<TrieInsertionResult (Trie&, TrieTestPair)>;
using TrieInsertionWithUriOp =
    std::function<TrieInsertionResult (Trie&, const std::string&, int)>;

//------------------------------------------------------------------------------
void checkWildcardTrieInsertion(const TrieTestPairList& pairs, bool clobbers,
                                TrieInsertionOp op)
{
    WildcardTrie<int> trie;
    for (unsigned i=0; i<pairs.size(); ++i)
    {
        INFO( "for pairs[" << i << "]" );
        const auto& pair = pairs[i];
        auto result = op(trie, pair);
        CHECK(result.second);
        CHECK(*result.first == pair.second);
        CHECK(result.first.value() == pair.second);
        CHECK(result.first.key() == pair.first);
        CHECK(result.first == trie.find(pair.first));
    }
    checkWildcardTrieContents(trie, pairs);

    // Check duplicate insertions
    for (unsigned i=0; i<pairs.size(); ++i)
    {
        INFO( "for pairs[" << i << "]" );
        auto pair = pairs[i];
        pair.second = -pair.second;
        auto result = op(trie, pair);
        CHECK_FALSE(result.second);
        CHECK(result.first.key() == pair.first);
        if (!clobbers)
            pair.second = -pair.second;
        CHECK(*result.first == pair.second);
        CHECK(result.first.value() == pair.second);
    }
}

//------------------------------------------------------------------------------
void checkWildcardTrieInsertionWithUri(const TrieTestPairList& pairs,
                                       bool clobbers, TrieInsertionWithUriOp op)
{
    WildcardTrie<int> trie;
    for (unsigned i=0; i<pairs.size(); ++i)
    {
        INFO( "for pairs[" << i << "]" );
        const auto& pair = pairs[i];
        auto result = op(trie, untokenizeUri(pair.first), pair.second);
        CHECK(result.second);
        CHECK(*result.first == pair.second);
        CHECK(result.first.value() == pair.second);
        CHECK(result.first.key() == pair.first);
        CHECK(result.first == trie.find(pair.first));
    }
    checkWildcardTrieContents(trie, pairs);

    // Check duplicate insertions
    for (unsigned i=0; i<pairs.size(); ++i)
    {
        INFO( "for pairs[" << i << "]" );
        auto pair = pairs[i];
        pair.second = -pair.second;
        auto result = op(trie, untokenizeUri(pair.first), pair.second);
        CHECK_FALSE(result.second);
        CHECK(result.first.key() == pair.first);
        if (!clobbers)
            pair.second = -pair.second;
        CHECK(*result.first == pair.second);
        CHECK(result.first.value() == pair.second);
    }
}

//------------------------------------------------------------------------------
void checkBadWildcardTrieAccess(const std::string& info,
                                const TrieTestPairList& pairs,
                                const SplitUri& key)
{
    INFO(info);
    SplitUri emptyKey;
    auto uri = untokenizeUri(key);
    Trie t(pairs.begin(), pairs.end());
    const Trie& c = t;
    CHECK_THROWS_AS(t.at(emptyKey), std::out_of_range);
    CHECK_THROWS_AS(c.at(emptyKey), std::out_of_range);
    CHECK_THROWS_AS(t.at(key), std::out_of_range);
    CHECK_THROWS_AS(c.at(key), std::out_of_range);
    CHECK_THROWS_AS(t.at(uri), std::out_of_range);
    CHECK_THROWS_AS(c.at(uri), std::out_of_range);
    CHECK(t.find(emptyKey) == t.end());
    CHECK(c.find(emptyKey) == c.end());
    CHECK(t.find(key) == t.end());
    CHECK(c.find(key) == c.end());
    CHECK(t.find(uri) == t.end());
    CHECK(c.find(uri) == c.end());
    CHECK(c.count(emptyKey) == 0);
    CHECK(c.count(key) == 0);
    CHECK(c.count(uri) == 0);
    CHECK_FALSE(c.contains(emptyKey));
    CHECK_FALSE(c.contains(key));
    CHECK_FALSE(c.contains(uri));
}

} // anonymous namespace


//------------------------------------------------------------------------------
TEST_CASE( "URI Tokenization", "[WildcardTrie]" )
{
    std::vector<std::pair<std::string, SplitUri>> inputs =
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
        CHECK(tokenizeUri(uri) == labels);
        CHECK(untokenizeUri(labels) == uri);
    }
}

//------------------------------------------------------------------------------
TEST_CASE( "URI Wildcard Matching", "[WildcardTrie]" )
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
            bool uriMatches = uriMatchesWildcardPattern(tokenizeUri(uri),
                                                        tokenizeUri(pattern));
            bool expected = matches.count(pattern) == 1;
            CHECK(uriMatches == expected);
        }
    }
}

//------------------------------------------------------------------------------
TEST_CASE( "Empty WildcardTrie Construction", "[WildcardTrie]" )
{
    Trie empty;

    SECTION( "default contruction" )
    {
        checkEmptyWildcardTrie(empty);
    };

    SECTION( "via iterator range" )
    {
        std::map<SplitUri, int> m;
        Trie trie(m.begin(), m.end());
        checkEmptyWildcardTrie(trie);
    };

    SECTION( "via initializer list" )
    {
        Trie trie({});
        checkEmptyWildcardTrie(trie);
    };

    SECTION( "via copy constructor" )
    {
        Trie b(empty);
        checkEmptyWildcardTrie(empty);
        checkEmptyWildcardTrie(b);
    };

    SECTION( "via move constructor" )
    {
        Trie b(std::move(empty));
        checkEmptyWildcardTrie(empty);
        checkEmptyWildcardTrie(b);
    };

    SECTION( "via copy assignment" )
    {
        Trie b{{{"a"}, 1}};
        b = empty;
        checkEmptyWildcardTrie(empty);
        checkEmptyWildcardTrie(b);
    };

    SECTION( "via move assignment" )
    {
        Trie b{{{"a"}, 1}};
        b = std::move(empty);
        checkEmptyWildcardTrie(empty);
        checkEmptyWildcardTrie(b);
    };
}

//------------------------------------------------------------------------------
TEST_CASE( "WildcardTrie Insertion", "[WildcardTrie]" )
{
    using Pair = TrieTestPair;

    std::vector<TrieTestPairList> inputs =
    {
        { {{""},            1} },
        { {{"a"},           1} },
        { {{"a", "b"},      1} },
        { {{"a", "b", "c"}, 1} },
        { {{"a"}, 1},           {{"b"}, 2}},
        { {{"b"}, 1},           {{"a"}, 2}},
        { {{"a"}, 1},           {{"a", "b"}, 2}},
        { {{"a", "b"}, 1},      {{"a"}, 2}},
        { {{"a", "b"}, 1},      {{"b"}, 2}},
        { {{"a", "b"}, 1},      {{"b", "a"}, 2}},
        { {{"a", "b"}, 1},      {{"c", "d"}, 2}},
        { {{"a", "b", "c"}, 1}, {{"a"}, 2}},
        { {{"a", "b", "c"}, 1}, {{"b"}, 2}},
        { {{"a", "b", "c"}, 1}, {{"c"}, 2}},
        { {{"a", "b", "c"}, 1}, {{"d"}, 2}},
        { {{"a", "b", "c"}, 1}, {{"a", "b"}, 2}},
        { {{"a", "b", "c"}, 1}, {{"b", "c"}, 2}},
        { {{"a", "b", "c"}, 1}, {{"d", "e"}, 2}},
        { {{"a", "b", "c"}, 1}, {{"a", "b", "d"}, 2}},
        { {{"a", "b", "c"}, 1}, {{"a", "d", "e"}, 2}},
        { {{"a", "b", "c"}, 1}, {{"d", "e", "f"}, 2}},
        { {{"d"}, 4}, {{"a"}, 1}, {{"c"}, 3}, {{"b"}, 2}, {{"e"}, 5}},
    };

    for (unsigned i=0; i<inputs.size(); ++i)
    {
        const auto& input = inputs[i];
        INFO( "for inputs[" << i << "]" );

        SECTION( "via constuctor taking iterator range" )
        {
            Trie trie(input.begin(), input.end());
            checkWildcardTrieContents(trie, input);
        };

        SECTION( "via insert pair" )
        {
            checkWildcardTrieInsertion(input, false,
                [](Trie& t, Pair p) {return t.insert(p);});
        };

        SECTION( "via insert moved pair" )
        {
            checkWildcardTrieInsertion(input, false,
                [](Trie& t, Pair p) {return t.insert(Pair{p});});
        };

        SECTION( "via insert_or_assign" )
        {
            checkWildcardTrieInsertion(input, true,
                [](Trie& t, Pair p)
                {
                    return t.insert_or_assign(p.first, p.second);
                });
        };

        SECTION( "via insert_or_assign with moved key" )
        {
            checkWildcardTrieInsertion(input, true,
                [](Trie& t, Pair p)
                {
                    return t.insert_or_assign(std::move(p.first), p.second);
                });
        };

        SECTION( "via insert_or_assign with URI string" )
        {
            checkWildcardTrieInsertionWithUri(input, true,
                [](Trie& t, const std::string& uri, int value)
                {
                    return t.insert_or_assign(uri, value);
                });
        };

        SECTION( "via emplace" )
        {
            checkWildcardTrieInsertion(input, false,
                [](Trie& t, Pair p) {return t.emplace(p.first, p.second);});
        };

        SECTION( "via try_emplace" )
        {
            checkWildcardTrieInsertion(input, false,
                [](Trie& t, Pair p) {return t.try_emplace(p.first, p.second);});
        };

        SECTION( "via try_emplace with moved key" )
        {
            checkWildcardTrieInsertion(input, false,
                [](Trie& t, Pair p)
                {
                    return t.try_emplace(std::move(p.first), p.second);
                });
        };

        SECTION( "via try_emplace with URI string" )
        {
            checkWildcardTrieInsertionWithUri(input, false,
            [](Trie& t, const std::string& uri, int value)
            {
                return t.try_emplace(uri, value);
            });
        };

        SECTION( "via operator[]" )
        {
            checkWildcardTrieInsertion(input, true,
                [](Trie& t, Pair p)
                {
                    bool inserted = t.find(p.first) == t.end();
                    t[p.first] = p.second;
                    return std::make_pair(t.find(p.first), inserted);
                });
        };

        SECTION( "via operator[] with moved key" )
        {
            checkWildcardTrieInsertion(input, true,
                [](Trie& t, Pair p)
                {
                    bool inserted = t.find(p.first) == t.end();
                    t[std::move(p.first)] = p.second;
                    return std::make_pair(t.find(p.first), inserted);
                });
        };

        SECTION( "via operator[] with URI string" )
        {
            checkWildcardTrieInsertionWithUri(input, true,
            [](Trie& t, const std::string& uri, int value)
            {
                bool inserted = t.find(uri) == t.end();
                t[uri] = value;
                return std::make_pair(t.find(uri), inserted);
            });
        };
    }
}

//------------------------------------------------------------------------------
TEST_CASE( "WildcardTrie Inializer Lists", "[WildcardTrie]" )
{
    TrieTestPairList pairs({ {{"a", "b", "c"}, 1}, {{"a"}, 2} });

    SECTION( "constuctor taking initializer list" )
    {
        Trie trie({ {{"a", "b", "c"}, 1}, {{"a"}, 2} });
        checkWildcardTrieContents(trie, pairs);
    };

    SECTION( "assignment from initializer list" )
    {
        Trie trie({ {{"z"}, 3} });
        trie = { {{"a", "b", "c"}, 1}, {{"a"}, 2} };
        checkWildcardTrieContents(trie, pairs);
    };

    SECTION( "assignment from empty initializer list" )
    {
        Trie trie({ {{"z"}, 3} });
        trie = {};
        checkEmptyWildcardTrie(trie);
    };
}

//------------------------------------------------------------------------------
TEST_CASE( "WildcardTrie Copy/Move Construction/Assignment", "[WildcardTrie]" )
{
    std::vector<TrieTestPairList> inputs = {
        { },
        { {{"a"},           1} },
        { {{"a", "b", "c"}, 1}, {{"a", "b"}, 2}},
        { {{"a", "b", "c"}, 1}, {{"d", "e"}, 2}},
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
            checkWildcardTrieContents(a, input);
            checkWildcardTrieContents(b, input);

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
            checkEmptyWildcardTrie(a);
            checkWildcardTrieContents(b, input);

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
            checkWildcardTrieContents(a, input);
            checkWildcardTrieContents(b, input);

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
            checkWildcardTrieContents(a, input);
            checkWildcardTrieContents(b, input);

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
            checkEmptyWildcardTrie(a);
            checkWildcardTrieContents(b, input);

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
            checkEmptyWildcardTrie(a);
            checkWildcardTrieContents(b, input);

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
TEST_CASE( "WildcardTrie Self-Assignment", "[WildcardTrie]" )
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
        CHECK(begin.uri() == "a");
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
        CHECK(begin.uri() == "a");
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
TEST_CASE( "Reusing Moved WildcardTrie", "[WildcardTrie]" )
{
    TrieTestPairList pairs({ {{"a", "b", "c"}, 1}, {{"a"}, 2} });
    Trie a({ {{"d"}, 3} });

    SECTION( "move constructor" )
    {
        Trie b(std::move(a));
        checkEmptyWildcardTrie(a);
        a.insert(pairs.begin(), pairs.end());
        checkWildcardTrieContents(a, pairs);
    }

    SECTION( "move assignment" )
    {
        Trie b;
        b = (std::move(a));
        checkEmptyWildcardTrie(a);
        a.insert(pairs.begin(), pairs.end());
        checkWildcardTrieContents(a, pairs);
    }
}

//------------------------------------------------------------------------------
TEST_CASE( "WildcardTrie Bad Access/Lookups", "[WildcardTrie]" )
{
    auto check = [](const std::string& info, const TrieTestPairList& pairs,
                    const SplitUri& key)
    {
        checkBadWildcardTrieAccess(info, pairs, key);
    };

    check("empty trie",        {},                {"a"});
    check("populated trie",    {{{"a"}, 1}},      {"b"});
    check("key is wildcard",   {{{"a"}, 1}},      {""});
    check("trie has wildcard", {{{""}, 1}},       {"a"});
    check("key is prefix",     {{{"a", "b"}, 1}}, {"a"});
    check("key is partial",    {{{"a", "b"}, 1}}, {"a", "c"});
    check("key too long",      {{{"a"}, 1}},      {"a", "b"});
}

//------------------------------------------------------------------------------
TEST_CASE( "WildcardTrie Pattern Matching", "[WildcardTrie]" )
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

    WildcardTrie<std::string> trie;
    for (const auto& pattern: patterns)
        trie.insert_or_assign(pattern, pattern);

    for (unsigned i=0; i<inputs.size(); ++i)
    {
        INFO( "for input[" << i << "]" );
        auto uri = inputs[i].first;
        auto key = tokenizeUri(uri);
        auto expectedHits = inputs[i].second;

        auto range = trie.match_range(key);
        auto match = range.first;
        std::set<std::string> hits;
        for (unsigned i = 0; i != expectedHits.size(); ++i)
        {
            REQUIRE(match != range.second);
            auto matchUri = untokenizeUri(match.key());
            CHECK(match.uri() == matchUri);
            CHECK( match.value() == matchUri );
            CHECK( *match == matchUri );
            REQUIRE( hits.emplace(matchUri).second );
            ++match;
        }
        CHECK(match == range.second);
        CHECK(hits == expectedHits);

        range = trie.match_range(uri);
        match = range.first;
        hits.clear();
        for (unsigned i = 0; i != expectedHits.size(); ++i)
        {
            REQUIRE( match != range.second );
            auto matchUri = untokenizeUri(match.key());
            CHECK( match.uri() == matchUri );
            CHECK( match.value() == matchUri );
            CHECK( *match == matchUri );
            REQUIRE( hits.emplace(matchUri).second );
            ++match;
        }
        CHECK( match == range.second );
        CHECK(hits == expectedHits);
    }
}

//------------------------------------------------------------------------------
TEST_CASE( "WildcardTrie Erase", "[WildcardTrie]" )
{
    Trie trie({ {{"a"}, 1}, {{"b"}, 2}, {{"b", "c"}, 3} });

    SECTION( "erasing via iterator" )
    {
        auto pos = trie.find("b.c");
        REQUIRE(pos.uri() == "b.c");
        auto iter = trie.erase(pos);
        CHECK(iter == trie.end());
        CHECK(trie.size() == 2);
        CHECK(trie.find("b.c") == trie.end());
        CHECK_FALSE(trie.contains("b.c"));

        pos = trie.find("a");
        REQUIRE(pos.uri() == "a");
        iter = trie.erase(pos);
        CHECK(iter != trie.end());
        CHECK(iter.uri() == "b");
        CHECK(trie.size() == 1);
        CHECK(trie.find("a") == trie.end());
        CHECK_FALSE(trie.contains("a"));

        // Re-insert last deleted key and erase it again
        auto inserted = trie.try_emplace("a", 1);
        REQUIRE(inserted.second);
        CHECK(trie.contains("a"));
        iter = trie.erase(inserted.first);
        CHECK(iter != trie.end());
        CHECK(iter.uri() == "b");
        CHECK(trie.size() == 1);
        CHECK(trie.find("a") == trie.end());
        CHECK_FALSE(trie.contains("a"));

        iter = trie.erase(trie.begin());
        CHECK(iter == trie.end());
        CHECK(trie.empty());
        CHECK(trie.find("b") == trie.end());
        CHECK_FALSE(trie.contains("b"));
    }

    SECTION( "erasing via key" )
    {
        bool erased = trie.erase("z");
        CHECK_FALSE(erased);
        CHECK(trie.size() == 3);

        erased = trie.erase("b.c");
        CHECK(erased);
        CHECK(trie.size() == 2);
        CHECK(trie.find("b.c") == trie.end());
        CHECK_FALSE(trie.contains("b.c"));

        erased = trie.erase("a");
        CHECK(erased);
        CHECK(trie.size() == 1);
        CHECK(trie.find("a") == trie.end());
        CHECK_FALSE(trie.contains("a"));

        erased = trie.erase("a");
        CHECK_FALSE(erased);
        CHECK(trie.size() == 1);

        // Re-insert last deleted key and erase it again
        auto inserted = trie.try_emplace("a", 1);
        REQUIRE(inserted.second);
        CHECK(trie.contains("a"));
        erased = trie.erase("a");
        CHECK(erased);
        CHECK(trie.size() == 1);
        CHECK(trie.find("a") == trie.end());
        CHECK_FALSE(trie.contains("a"));

        erased = trie.erase("b");
        CHECK(erased);
        CHECK(trie.empty());
        CHECK(trie.find("b") == trie.end());
        CHECK_FALSE(trie.contains("b"));
    }

    // TODO: Check tree pruning
}

//------------------------------------------------------------------------------
TEST_CASE( "WildcardTrie Clear", "[WildcardTrie]" )
{
    SECTION("non-empty trie")
    {
        Trie t({ {{"a"}, 1} });
        t.clear();
        checkEmptyWildcardTrie(t);
        t.clear();
        checkEmptyWildcardTrie(t);
    }

    SECTION("default-constructed trie")
    {
        Trie t;
        t.clear();
        checkEmptyWildcardTrie(t);
        t.clear();
        checkEmptyWildcardTrie(t);
    }
}

//------------------------------------------------------------------------------
TEST_CASE( "WildcardTrie Swap", "[WildcardTrie]" )
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
        CHECK(aBegin.uri() == "a");
        CHECK(++Trie::iterator(aBegin) == b.end());
        CHECK(b.size() == 1);
        CHECK(b.contains("a"));
        CHECK(bBegin == a.begin());
        REQUIRE(bBegin != a.end());
        CHECK(bBegin.uri() == "b");
        CHECK((++Trie::iterator(bBegin)).uri() == "c");
        CHECK(++(++Trie::iterator(bBegin)) == a.end());

        swap(b, a);
        CHECK(a.size() == 1);
        CHECK(a.contains("a"));
        CHECK(aBegin == a.begin());
        REQUIRE(aBegin != a.end());
        CHECK(aBegin.uri() == "a");
        CHECK(++Trie::iterator(aBegin) == a.end());
        CHECK(b.size() == 2);
        CHECK(b.contains("b"));
        CHECK(b.contains("c"));
        CHECK(bBegin == b.begin());
        REQUIRE(bBegin != b.end());
        CHECK(bBegin.uri() == "b");
        CHECK((++Trie::iterator(bBegin)).uri() == "c");
        CHECK(++(++Trie::iterator(bBegin)) == b.end());
    }

    SECTION("rhs trie is empty")
    {
        a.swap(x);
        CHECK(a.empty());
        CHECK(aBegin == x.begin());
        REQUIRE(aBegin != x.end());
        CHECK(aBegin.uri() == "a");
        CHECK(++Trie::iterator(aBegin) == x.end());
        CHECK(x.size() == 1);
        CHECK(x.contains("a"));

        swap(x, a);
        CHECK(a.size() == 1);
        CHECK(a.contains("a"));
        CHECK(aBegin == a.begin());
        CHECK(++Trie::iterator(aBegin) == a.end());
        REQUIRE(aBegin != a.end());
        CHECK(aBegin.uri() == "a");
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
        CHECK(aBegin.uri() == "a");
        CHECK(++Trie::iterator(aBegin) == x.end());

        swap(a, x);
        CHECK(a.size() == 1);
        CHECK(a.contains("a"));
        CHECK(aBegin == a.begin());
        REQUIRE(aBegin != a.end());
        CHECK(aBegin.uri() == "a");
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
        CHECK(aBegin.uri() == "a");
        CHECK(++Trie::iterator(aBegin) == a.end());

        swap(b, b);
        CHECK(b.size() == 2);
        CHECK(b.contains("b"));
        CHECK(b.contains("c"));
        CHECK(bBegin == b.begin());
        REQUIRE(bBegin != b.end());
        CHECK(bBegin.uri() == "b");
        CHECK((++Trie::iterator(bBegin)).uri() == "c");
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
TEST_CASE( "WildcardTrie Modification Preserves Iterators", "[WildcardTrie]" )
{
    // TODO: Various erases and insertions
}

//------------------------------------------------------------------------------
TEST_CASE( "WildcardTrie Iterator Conversions", "[WildcardTrie]" )
{
    // TODO
}

//------------------------------------------------------------------------------
TEST_CASE( "WildcardTrie Mixed Iterator Comparisons", "[WildcardTrie]" )
{
    // TODO
}
