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

using TrieTestPairs = std::vector<std::pair<const SplitUri, int>>;

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
void checkWildcardTrieContents(WildcardTrie<T>& t, const TrieTestPairs& pairs)
{
    std::map<SplitUri, T> m(pairs.begin(), pairs.end());
    const WildcardTrie<T>& c = t;
    CHECK(c.empty() == m.empty());
    CHECK(c.size() == m.size());
    CHECK(c.begin() != c.end());
    CHECK(t.begin() != t.end());
    CHECK(t.cbegin() != t.cend());

    auto ti = t.begin();
    auto mi = m.begin();
    for (unsigned i=0; i<m.size(); ++i)
    {
        auto key = mi->first;
        auto value = mi->second;
        INFO("at position " << i);
        CHECK(*ti == value);
        CHECK(ti.value() == value);
        CHECK(ti.key() == key);
        CHECK(c.at(key) == value);
        CHECK(t.at(key) == value);
        CHECK(t[key] == value);
        CHECK(t[SplitUri{key}] == value);
        CHECK(c.count(key) == 1);
        CHECK(c.contains(key));

        auto cf = c.find(key);
        REQUIRE( cf != c.end() );
        CHECK( *cf == value );
        CHECK( cf.key() == key );
        CHECK( cf.value() == value );

        auto mf = t.find(key);
        REQUIRE( mf != t.end() );
        CHECK( *mf == value );
        CHECK( mf.key() == key );
        CHECK( mf.value() == value );

        ++ti;
        ++mi;
    }
}

//------------------------------------------------------------------------------
using TrieInsertionOp =
    std::function<
        std::pair<WildcardTrie<int>::iterator, bool>
            (WildcardTrie<int>&, std::pair<const SplitUri, int>)>;

//------------------------------------------------------------------------------
void checkWildcardTrieInsertion(const TrieTestPairs& pairs, bool clobbers,
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

} // anonymous namespace

//------------------------------------------------------------------------------
TEST_CASE( "URI Tokenization", "[WildcardTrie]" )
{
    std::vector<std::pair<std::string, SplitUri>> inputs =
    {
        {"",      {}},
        {".",     {"", ""}},
        {"..",    {"", "", ""}},
        {"..a",   {"", "", "a"}},
        {".a",    {"", "a"}},
        {".a.",   {"", "a", ""}},
        {".a..",  {"", "a", "", ""}},
        {".a.b",  {"", "a", "b"}},
        {"a",     {"a"}},
        {"a.",    {"a", ""}},
        {"a..",   {"a", "", ""}},
        {"a..b",  {"a", "", "b"}},
        {"a.b",   {"a", "b"}},
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
        {"abc",     {}},
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
    using Pair = std::pair<const SplitUri, int>;
    using Pairs = std::vector<Pair>;

    std::vector<Pairs> inputs =
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
    }
}

//------------------------------------------------------------------------------
TEST_CASE( "WildcardTrie Inializer Lists", "[WildcardTrie]" )
{
    using Pair = std::pair<const SplitUri, int>;
    using Pairs = std::vector<Pair>;

    Pairs pairs({ {{"a", "b", "c"}, 1}, {{"a"}, 2} });

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
    using Pair = std::pair<const SplitUri, int>;
    using Pairs = std::vector<Pair>;

    std::vector<Pairs> inputs = {
        { {{"a"},           1} },
        { {{"a", "b", "c"}, 1}, {{"a", "b"}, 2}},
        { {{"a", "b", "c"}, 1}, {{"d", "e"}, 2}},
    };

    for (unsigned i=0; i<inputs.size(); ++i)
    {
        INFO("for input[" << i << "]");
        const auto& input = inputs[i];
        Trie a(input.begin(), input.end());

        SECTION( "copy construction" )
        {
            Trie b(a);
            checkWildcardTrieContents(a, input);
            checkWildcardTrieContents(b, input);
        };

        SECTION( "move construction" )
        {
            Trie b(std::move(a));
            checkEmptyWildcardTrie(a);
            checkWildcardTrieContents(b, input);
        };

        SECTION( "copy assignment" )
        {
            Trie b;
            b = a;
            checkWildcardTrieContents(a, input);
            checkWildcardTrieContents(b, input);
        };

        SECTION( "move assignment" )
        {
            Trie b;
            b = std::move(a);
            checkEmptyWildcardTrie(a);
            checkWildcardTrieContents(b, input);
        };
    }
}

//------------------------------------------------------------------------------
TEST_CASE( "WildcardTrie Test Case", "[WildcardTrie]" )
{
    SECTION( "some section" )
    {
    };
}

