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

using Trie = WildcardTrie<int>;

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

template <typename T>
void checkWildcardTrieContents(WildcardTrie<T>& t,
                               const std::map<SplitUri, T>& m)
{
    const WildcardTrie<T>& c = t;
    CHECK(c.empty() == m.empty());
    CHECK(c.size() == m.size());
    CHECK(c.begin() != c.end());
    CHECK(t.begin() != t.end());
    CHECK(t.cbegin() != t.cend());
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
TEST_CASE( "WildcardTrie Construction With a Single Element", "[WildcardTrie]" )
{
    std::map<SplitUri, int> m({ {{"foo"}, 42} });

    SECTION( "via iterator range" )
    {
        Trie trie(m.begin(), m.end());
        checkWildcardTrieContents(trie, m);
    };

    SECTION( "via initializer list" )
    {
        Trie trie({ {{"foo"}, 42} });
        checkWildcardTrieContents(trie, m);
    };
}

//------------------------------------------------------------------------------
TEST_CASE( "WildcardTrie Test Case", "[WildcardTrie]" )
{
    SECTION( "some section" )
    {
    };
}

