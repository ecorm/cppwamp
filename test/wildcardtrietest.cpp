/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include <cppwamp/internal/trie.hpp>
#include <catch2/catch.hpp>
#include <map>

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
void checkWildcardTrieInsertion(const TrieTestPairs& pairs, TrieInsertionOp op)
{
    WildcardTrie<int> trie;
    for (unsigned i=0; i<pairs.size(); ++i)
    {
        INFO( "for pairs[" << i << "]" );
        const auto& pair = pairs[i];
        auto result = op(trie, pair);
        CHECK(result.second);
        CHECK(result.first == trie.find(pair.first));
    }
    checkWildcardTrieContents(trie, pairs);
}

} // anonymous namespace

//------------------------------------------------------------------------------
TEST_CASE( "Empty WildcardTrie Construction", "[WildcardTrie]" )
{
    SECTION( "default contruction" )
    {
        Trie trie;
        checkEmptyWildcardTrie(trie);
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
}

//------------------------------------------------------------------------------
TEST_CASE( "WildcardTrie Insertion", "[WildcardTrie]" )
{
    using Pair = std::pair<const SplitUri, int>;
    using Pairs = std::vector<Pair>;

    std::vector<Pairs> inputs =
    {
        { {{""}, 1} },
        { {{"a"}, 1} },
        { {{"a", "b"}, 1} },
        { {{"a"}, 1}, {{"b"}, 2}},
        { {{"b"}, 1}, {{"a"}, 2}},
        { {{"a"}, 1}, {{"a", "b"}, 2}},
        { {{"a", "b"}, 1}, {{"a"}, 2}},
        { {{"a", "b"}, 1}, {{"b"}, 2}},
        { {{"a", "b"}, 1}, {{"b", "a"}, 2}},
        { {{"a", "b"}, 1}, {{"c", "d"}, 2}},
        { {{"a", "b", "c"}, 1}, {{"a"}, 2}},
        { {{"a", "b", "c"}, 1}, {{"b"}, 2}},
        { {{"a", "b", "c"}, 1}, {{"c"}, 2}},
        { {{"a", "b", "c"}, 1}, {{"a", "b"}, 2}},
        { {{"a", "b", "c"}, 1}, {{"b", "c"}, 2}},
        { {{"a", "b", "c"}, 1}, {{"a", "b", "d"}, 2}},
        { {{"a", "b", "c"}, 1}, {{"a", "d", "e"}, 2}},
        { {{"a", "b", "c"}, 1}, {{"d"}, 2}},
        { {{"a", "b", "c"}, 1}, {{"d", "e"}, 2}},
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
            checkWildcardTrieInsertion(
                input,
                [](Trie& t, Pair p) {return t.insert(p);});
        };

        SECTION( "via insert moved pair" )
        {
            checkWildcardTrieInsertion(
                input,
                [](Trie& t, Pair p) {return t.insert(Pair{p});});
        };

        SECTION( "via insert_or_assign" )
        {
            checkWildcardTrieInsertion(
                input,
                [](Trie& t, Pair p)
                {
                    return t.insert_or_assign(p.first, p.second);
                });
        };

        SECTION( "via insert_or_assign with moved key" )
        {
            checkWildcardTrieInsertion(
                input,
                [](Trie& t, Pair p)
                {
                    return t.insert_or_assign(SplitUri{p.first}, p.second);
                });
        };

        SECTION( "via emplace" )
        {
            checkWildcardTrieInsertion(
                input,
                [](Trie& t, Pair p) {return t.emplace(p.first, p.second);});
        };

        SECTION( "via try_emplace" )
        {
            checkWildcardTrieInsertion(
                input,
                [](Trie& t, Pair p) {return t.try_emplace(p.first, p.second);});
        };

        SECTION( "via try_emplace with moved key" )
        {
            checkWildcardTrieInsertion(
                input,
                [](Trie& t, Pair p)
                {
                    return t.try_emplace(SplitUri{p.first}, p.second);
                });
        };
    }

    SECTION( "via constuctor taking initializer list" )
    {
        Trie trie({ {{"a", "b", "c"}, 1}, {{"a"}, 2} });
        Pairs pairs({ {{"a", "b", "c"}, 1}, {{"a"}, 2} });
        checkWildcardTrieContents(trie, pairs);
    };
}

//------------------------------------------------------------------------------
TEST_CASE( "WildcardTrie Test Case", "[WildcardTrie]" )
{
    SECTION( "some section" )
    {
    };
}

