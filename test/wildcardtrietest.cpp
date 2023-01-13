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

using TrieTestVector = std::initializer_list<std::pair<SplitUri, int>>;

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
void checkWildcardTrieContents(WildcardTrie<T>& t, std::map<SplitUri, T> m)
{
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
        INFO("at position " << i);
        CHECK(ti.value() == mi->second);
        CHECK(*ti == mi->second);
        CHECK(ti.key() == mi->first);
        ++ti;
        ++mi;
    }
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
    using Map = std::map<SplitUri, int>;

    std::vector<Map> maps =
    {
        { {{""}, 0} },
        { {{"foo"}, 1} },
        { {{"foo", "bar"}, 2} },
    };

    for (unsigned i=0; i<maps.size(); ++i)
    {
        const auto& map = maps[i];
        INFO( "for maps[" << i << "]" );
        SECTION( "via constuctor taking iterator range" )
        {
            Trie trie(map.begin(), map.end());
            checkWildcardTrieContents(trie, map);
        };
    }

    SECTION( "via constuctor taking initializer list" )
    {
        Trie trie({ {{"foo"}, 1} });
        checkWildcardTrieContents(trie, maps.at(1));
    };
}

//------------------------------------------------------------------------------
TEST_CASE( "WildcardTrie Test Case", "[WildcardTrie]" )
{
    SECTION( "some section" )
    {
    };
}

