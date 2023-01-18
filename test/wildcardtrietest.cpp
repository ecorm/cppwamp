/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include <cppwamp/tokentrie.hpp>
#include <cppwamp/wildcarduri.hpp>
#include <catch2/catch.hpp>
#include <set>
#include <map>
#include <vector>

using namespace wamp;
using namespace wamp::internal;

namespace
{

//------------------------------------------------------------------------------
using Trie = TokenTrie<SplitUri, int>;
using TrieTestPair = std::pair<const SplitUri, int>;
using TrieTestPairList = std::vector<TrieTestPair>;

//------------------------------------------------------------------------------
template <typename K, typename T>
void checkEmptyTokenTrie(TokenTrie<K, T>& t)
{
    const TokenTrie<K, T>& c = t;
    CHECK(c.empty());
    CHECK(c.size() == 0);
    CHECK(c.begin() == c.end());
    CHECK(t.begin() == t.end());
    CHECK(t.cbegin() == t.cend());
}

//------------------------------------------------------------------------------
template <typename K, typename T>
void checkTokenTrieContents(TokenTrie<K, T>& t, const TrieTestPairList& pairs)
{
    if (pairs.empty())
        return checkEmptyTokenTrie(t);

    std::map<SplitUri, T> m(pairs.begin(), pairs.end());
    const TokenTrie<K, T>& c = t;
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
        CHECK( t[key] == value );
        CHECK( t[SplitUri{key}] == value );
        CHECK( c.count(key) == 1 );
        CHECK( c.contains(key) );

        auto mf = t.find(key);
        REQUIRE( mf != t.end() );
        CHECK( *mf == value );
        CHECK( mf.key() == key );
        CHECK( mf.value() == value );

        REQUIRE( mf != t.end() );
        CHECK( *mf == value );
        CHECK( mf.key() == key );
        CHECK( mf.value() == value );

        auto cf = c.find(key);
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
void checkTokenTrieInsertion(const TrieTestPairList& pairs, bool clobbers,
                             TrieInsertionOp op)
{
    Trie trie;
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
    checkTokenTrieContents(trie, pairs);

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
void checkBadTokenTrieAccess(const std::string& info,
                                const TrieTestPairList& pairs,
                                const SplitUri& key)
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
bool checkTokenTrieUris(const Trie& t, const std::vector<std::string>& uris)
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
bool checkTokenTrieIterators(const Trie& t,
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
void checkTokenTrieEqualRange(const Trie& t, const std::string& uri,
                                 const std::string& lbUri,
                                 const std::string& ubUri)
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
bool checkTokenTrieComparisons(const Trie& a, const Trie& b)
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
TEST_CASE( "URI Tokenization", "[TokenTrie]" )
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
        CHECK(s.unsplit() == uri);
    }
}

//------------------------------------------------------------------------------
TEST_CASE( "URI Wildcard Matching", "[TokenTrie]" )
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
            bool uriMatches = wildcardMatches(uri, pattern);
            bool expected = matches.count(pattern) == 1;
            CHECK(uriMatches == expected);
        }
    }
}

//------------------------------------------------------------------------------
TEST_CASE( "Empty TokenTrie Construction", "[TokenTrie]" )
{
    Trie empty;

    SECTION( "default contruction" )
    {
        checkEmptyTokenTrie(empty);
    };

    SECTION( "via iterator range" )
    {
        std::map<SplitUri, int> m;
        Trie trie(m.begin(), m.end());
        checkEmptyTokenTrie(trie);
    };

    SECTION( "via initializer list" )
    {
        Trie trie({});
        checkEmptyTokenTrie(trie);
    };

    SECTION( "via copy constructor" )
    {
        Trie b(empty);
        checkEmptyTokenTrie(empty);
        checkEmptyTokenTrie(b);
    };

    SECTION( "via move constructor" )
    {
        Trie b(std::move(empty));
        checkEmptyTokenTrie(empty);
        checkEmptyTokenTrie(b);
    };

    SECTION( "via copy assignment" )
    {
        Trie b{{{"a"}, 1}};
        b = empty;
        checkEmptyTokenTrie(empty);
        checkEmptyTokenTrie(b);
    };

    SECTION( "via move assignment" )
    {
        Trie b{{{"a"}, 1}};
        b = std::move(empty);
        checkEmptyTokenTrie(empty);
        checkEmptyTokenTrie(b);
    };
}

//------------------------------------------------------------------------------
TEST_CASE( "TokenTrie Insertion", "[TokenTrie]" )
{
    using Pair = TrieTestPair;

    std::vector<TrieTestPairList> inputs =
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
            checkTokenTrieContents(trie, input);
        };

        SECTION( "via constuctor taking special iterator range" )
        {
            Trie a(input.begin(), input.end());
            Trie b(a.begin(), a.end());
            checkTokenTrieContents(b, input);
        };

        SECTION( "via insert iterator range" )
        {
            Trie trie;
            trie.insert(input.begin(), input.end());
            checkTokenTrieContents(trie, input);
        };

        SECTION( "via insert special iterator range" )
        {
            Trie a(input.begin(), input.end());
            Trie b;
            b.insert(a.begin(), a.end());
            checkTokenTrieContents(b, input);
        };

        SECTION( "via insert pair" )
        {
            checkTokenTrieInsertion(input, false,
                [](Trie& t, Pair p) {return t.insert(p);});
        };

        SECTION( "via insert moved pair" )
        {
            checkTokenTrieInsertion(input, false,
                [](Trie& t, Pair p) {return t.insert(Pair{p});});
        };

        SECTION( "via insert_or_assign" )
        {
            checkTokenTrieInsertion(input, true,
                [](Trie& t, Pair p)
                {
                    return t.insert_or_assign(p.first, p.second);
                });
        };

        SECTION( "via insert_or_assign with moved key" )
        {
            checkTokenTrieInsertion(input, true,
                [](Trie& t, Pair p)
                {
                    return t.insert_or_assign(std::move(p.first), p.second);
                });
        };

        SECTION( "via emplace" )
        {
            checkTokenTrieInsertion(input, false,
                [](Trie& t, Pair p) {return t.emplace(p.first, p.second);});
        };

        SECTION( "via try_emplace" )
        {
            checkTokenTrieInsertion(input, false,
                [](Trie& t, Pair p) {return t.try_emplace(p.first, p.second);});
        };

        SECTION( "via try_emplace with moved key" )
        {
            checkTokenTrieInsertion(input, false,
                [](Trie& t, Pair p)
                {
                    return t.try_emplace(std::move(p.first), p.second);
                });
        };

        SECTION( "via operator[]" )
        {
            checkTokenTrieInsertion(input, true,
                [](Trie& t, Pair p)
                {
                    bool inserted = t.find(p.first) == t.end();
                    t[p.first] = p.second;
                    return std::make_pair(t.find(p.first), inserted);
                });
        };

        SECTION( "via operator[] with moved key" )
        {
            checkTokenTrieInsertion(input, true,
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
TEST_CASE( "TokenTrie Inializer Lists", "[TokenTrie]" )
{
    TrieTestPairList pairs({ {"a.b.c", 1}, {"a", 2} });

    SECTION( "constuctor taking initializer list" )
    {
        Trie trie({ {"a.b.c", 1}, {"a", 2} });
        checkTokenTrieContents(trie, pairs);
    };

    SECTION( "assignment from initializer list" )
    {
        Trie trie({ {"z", 3} });
        trie = { {"a.b.c", 1}, {"a", 2} };
        checkTokenTrieContents(trie, pairs);
    };

    SECTION( "assignment from empty initializer list" )
    {
        Trie trie({ {{"z"}, 3} });
        trie = {};
        checkEmptyTokenTrie(trie);
    };
}

//------------------------------------------------------------------------------
TEST_CASE( "TokenTrie Copy/Move Construction/Assignment", "[TokenTrie]" )
{
    std::vector<TrieTestPairList> inputs = {
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
            checkTokenTrieContents(a, input);
            checkTokenTrieContents(b, input);

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
            checkEmptyTokenTrie(a);
            checkTokenTrieContents(b, input);

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
            checkTokenTrieContents(a, input);
            checkTokenTrieContents(b, input);

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
            checkTokenTrieContents(a, input);
            checkTokenTrieContents(b, input);

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
            checkEmptyTokenTrie(a);
            checkTokenTrieContents(b, input);

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
            checkEmptyTokenTrie(a);
            checkTokenTrieContents(b, input);

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
TEST_CASE( "TokenTrie Self-Assignment", "[TokenTrie]" )
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
TEST_CASE( "Reusing Moved TokenTrie", "[TokenTrie]" )
{
    TrieTestPairList pairs({ {"a.b.c", 1}, {"a", 2} });
    Trie a({ {{"d"}, 3} });

    SECTION( "move constructor" )
    {
        Trie b(std::move(a));
        checkEmptyTokenTrie(a);
        a.insert(pairs.begin(), pairs.end());
        checkTokenTrieContents(a, pairs);
    }

    SECTION( "move assignment" )
    {
        Trie b;
        b = (std::move(a));
        checkEmptyTokenTrie(a);
        a.insert(pairs.begin(), pairs.end());
        checkTokenTrieContents(a, pairs);
    }
}

//------------------------------------------------------------------------------
TEST_CASE( "TokenTrie Bad Access/Lookups", "[TokenTrie]" )
{
    auto check = [](const std::string& info, const TrieTestPairList& pairs,
                    const SplitUri& key)
    {
        checkBadTokenTrieAccess(info, pairs, key);
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
TEST_CASE( "TokenTrie Lower/Upper Bound and Equal Range", "[TokenTrie]" )
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
            return checkTokenTrieEqualRange(t, uri, lbUri, ubUri);
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
TEST_CASE( "TokenTrie Pattern Matching", "[TokenTrie]" )
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

    TokenTrie<SplitUri, std::string> trie;
    for (const auto& pattern: patterns)
        trie.insert_or_assign(pattern, pattern);

    for (unsigned i=0; i<inputs.size(); ++i)
    {
        INFO( "for input[" << i << "]" );
        auto uri = inputs[i].first;
        auto key = SplitUri(uri);
        auto expectedHits = inputs[i].second;

        auto range = trie.match_range(key);
        auto match = range.first;
        std::set<std::string> hits;
        for (unsigned i = 0; i != expectedHits.size(); ++i)
        {
            REQUIRE(match != range.second);
            auto matchUri = match.key().unsplit().value();
            CHECK(match.key() == matchUri);
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
            auto matchUri = match.key().unsplit().value();
            CHECK( match.key() == matchUri );
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
TEST_CASE( "TokenTrie Insertion From Match Range", "[TokenTrie]" )
{
    Trie trie({ {"a", 1}, {"a.", 2}, {".b", 3} });

    SECTION( "constructor taking match range" )
    {
        auto range = trie.match_range("a.b");
        Trie matches(range.first, range.second);
        checkTokenTrieUris(matches, {".b", "a."});
    };

    SECTION( "insert taking match range" )
    {
        auto range = trie.match_range("a.b");
        Trie matches;
        matches.insert(range.first, range.second);
        checkTokenTrieUris(matches, {".b", "a."});
    };
}

//------------------------------------------------------------------------------
TEST_CASE( "TokenTrie Erase", "[TokenTrie]" )
{
    Trie trie({ {"a", 1}, {"a.b.c", 2}, {"d", 3}, {"d.e", 4} });
    auto rootNode = TokenTrieIteratorAccess::cursor(trie.begin()).node;
    REQUIRE(rootNode->isRoot());

    SECTION( "erasing via iterator" )
    {
        auto pos = trie.find("a.b.c");
        REQUIRE(pos != trie.end());
        auto iter = trie.erase(pos);
        CHECK(iter == trie.find("d"));
        CHECK(checkTokenTrieUris(trie, {"a", "d", "d.e"}));
        // Check pruning below "a" node
        auto cursor = TokenTrieIteratorAccess::cursor(trie.find("a"));
        CHECK(cursor.iter->second.children.empty());

        pos = trie.find("d");
        REQUIRE(pos != trie.end());
        iter = trie.erase(pos);
        CHECK(iter == trie.find("d.e"));
        CHECK(checkTokenTrieUris(trie, {"a", "d.e"}));
        // Check non-terminal "d" node still exists
        cursor = TokenTrieIteratorAccess::cursor(trie.find("d.e"));
        CHECK(cursor.node->position->first == "d");
        CHECK(!cursor.node->isTerminal);

        pos = trie.find("a");
        REQUIRE(pos != trie.end());
        iter = trie.erase(pos);
        CHECK(iter == trie.find("d.e"));
        CHECK(checkTokenTrieUris(trie, {"d.e"}));
        // Check root node has a single "d" child node
        REQUIRE(rootNode->children.size() == 1);
        CHECK(rootNode->children.begin()->first == "d");

        // Re-insert last deleted key and erase it again
        auto inserted = trie.try_emplace("a", 1);
        REQUIRE(inserted.second);
        CHECK(checkTokenTrieUris(trie, {"a", "d.e"}));
        iter = trie.erase(inserted.first);
        CHECK(iter == trie.find("d.e"));
        CHECK(checkTokenTrieUris(trie, {"d.e"}));
        // Check root node has a single "d" child node
        REQUIRE(rootNode->children.size() == 1);
        CHECK(rootNode->children.begin()->first == "d");

        pos = trie.find("d.e");
        REQUIRE(pos != trie.end());
        iter = trie.erase(pos);
        CHECK(iter == trie.end());
        CHECK(trie.empty());
        // Check root node has no child nodes
        CHECK(rootNode->children.empty());
    }

    SECTION( "erasing via key" )
    {
        CHECK_FALSE(trie.erase("z"));
        CHECK(checkTokenTrieUris(trie, {"a", "a.b.c", "d", "d.e"}));

        CHECK(trie.erase("a.b.c"));
        CHECK(checkTokenTrieUris(trie, {"a", "d", "d.e"}));

        CHECK(trie.erase("d"));
        CHECK(checkTokenTrieUris(trie, {"a", "d.e"}));

        CHECK(trie.erase("a"));
        CHECK(checkTokenTrieUris(trie, {"d.e"}));

        // Re-insert last deleted key and erase it again
        auto inserted = trie.try_emplace("a", 1);
        REQUIRE(inserted.second);
        CHECK(trie.erase("a"));
        CHECK(checkTokenTrieUris(trie, {"d.e"}));

        CHECK(trie.erase("d.e"));
        CHECK(trie.empty());
    }
}

//------------------------------------------------------------------------------
TEST_CASE( "TokenTrie Clear", "[TokenTrie]" )
{
    SECTION("non-empty trie")
    {
        Trie t({ {{"a"}, 1} });
        t.clear();
        checkEmptyTokenTrie(t);
        t.clear();
        checkEmptyTokenTrie(t);
    }

    SECTION("default-constructed trie")
    {
        Trie t;
        t.clear();
        checkEmptyTokenTrie(t);
        t.clear();
        checkEmptyTokenTrie(t);
    }
}

//------------------------------------------------------------------------------
TEST_CASE( "TokenTrie Swap", "[TokenTrie]" )
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
TEST_CASE( "TokenTrie Modification Preserves Iterators", "[TokenTrie]" )
{
    Trie t;
    auto z = t.end();
    auto b = t.insert_or_assign("b", 2).first;
    CHECK(checkTokenTrieIterators(t, {b, z}));
    auto a = t.insert_or_assign("a", 2).first;
    CHECK(checkTokenTrieIterators(t, {a, b, z}));
    auto d = t.insert_or_assign("d", 4).first;
    CHECK(checkTokenTrieIterators(t, {a, b, d, z}));
    auto bc = t.insert_or_assign("b.c", 3).first;
    CHECK(checkTokenTrieIterators(t, {a, b, bc, d, z}));
    t.erase("b");
    CHECK(checkTokenTrieIterators(t, {a, bc, d, z}));
    t.erase("a");
    CHECK(checkTokenTrieIterators(t, {bc, d, z}));
    t.erase("d");
    CHECK(checkTokenTrieIterators(t, {bc, z}));
    t.erase("b.c");
    CHECK(checkTokenTrieIterators(t, {z}));
}

//------------------------------------------------------------------------------
TEST_CASE( "TokenTrie Comparisons", "[TokenTrie]" )
{
    auto check = [](const Trie& a, const Trie& b) -> bool
    {
        return checkTokenTrieComparisons(a, b);
    };

    CHECK( check({{}},                      {{"a", 1}}) );
    CHECK( check({{"a",   1}},              {{"a", 2}}) );
    CHECK( check({{"a",   1}},              {{"b", 1}}) );
    CHECK( check({{"a.b", 1}},              {{"a", 1}}) );
    CHECK( check({{"a",   1}, {"b",   2}},  {{"a", 1}}) );
    CHECK( check({{"a",   1}, {"a.b", 2}},  {{"a.b", 2}}) );
}

//------------------------------------------------------------------------------
TEST_CASE( "TokenTrie erase_if", "[TokenTrie]" )
{
    Trie trie({{"a", 1}, {"b", 2}, {"b.c", 1}});

    SECTION( "criteria based on value" )
    {
        auto n = erase_if(
            trie,
            [](Trie::value_type kv) {return kv.second == 1;} );
        CHECK(n == 2);
        checkTokenTrieUris(trie, {"b"});
    }

    SECTION( "criteria based on key" )
    {
        auto n = erase_if(
            trie,
            [](Trie::value_type kv) {return kv.first.front() == "b";} );
        CHECK(n == 2);
        checkTokenTrieUris(trie, {"a"});
    }
}

//------------------------------------------------------------------------------
TEST_CASE( "TokenTrie Iterator Conversions and Mixed Comparisons",
           "[TokenTrie]" )
{
    using CI = Trie::const_iterator;
    using CM = Trie::const_match_iterator;
    using MI = Trie::iterator;
    using MM = Trie::match_iterator;

    CHECK(std::is_convertible<CI, CM>::value == false);
    CHECK(std::is_convertible<CI, MI>::value == false);
    CHECK(std::is_convertible<CI, MM>::value == false);
    CHECK(std::is_convertible<CM, CI>::value == true);
    CHECK(std::is_convertible<CM, MI>::value == false);
    CHECK(std::is_convertible<CM, MM>::value == false);
    CHECK(std::is_convertible<MI, CI>::value == true);
    CHECK(std::is_convertible<MI, CM>::value == false);
    CHECK(std::is_convertible<MI, MM>::value == false);
    CHECK(std::is_convertible<MM, CI>::value == true);
    CHECK(std::is_convertible<MM, CM>::value == true);
    CHECK(std::is_convertible<MM, MI>::value == true);

    Trie t({ {{"a"}, 1} });
    const auto& c = t;
    CI ci = t.cbegin();
    CM cm = c.match_range("a").first;
    MI mi = t.begin();
    MM mm = t.match_range("a").first;

    CHECK(CI(ci).key() == "a");
    CHECK(CI(cm).key() == "a");
    CHECK(CI(mi).key() == "a");
    CHECK(CI(mm).key() == "a");
    CHECK(CM(cm).key() == "a");
    CHECK(CM(mm).key() == "a");
    CHECK(MI(mi).key() == "a");
    CHECK(MI(mm).key() == "a");
    CHECK(MM(mm).key() == "a");

    CHECK((ci == ci));
    CHECK((ci == cm));
    CHECK((ci == mi));
    CHECK((ci == mm));
    CHECK((cm == ci));
    CHECK((cm == cm));
    CHECK((cm == mi));
    CHECK((cm == mm));
    CHECK((mi == ci));
    CHECK((mi == cm));
    CHECK((mi == mi));
    CHECK((mi == mm));
    CHECK((mm == ci));
    CHECK((mm == cm));
    CHECK((mm == mi));
    CHECK((mm == mm));

    CHECK_FALSE((ci != ci));
    CHECK_FALSE((ci != cm));
    CHECK_FALSE((ci != mi));
    CHECK_FALSE((ci != mm));
    CHECK_FALSE((cm != ci));
    CHECK_FALSE((cm != cm));
    CHECK_FALSE((cm != mi));
    CHECK_FALSE((cm != mm));
    CHECK_FALSE((mi != ci));
    CHECK_FALSE((mi != cm));
    CHECK_FALSE((mi != mi));
    CHECK_FALSE((mi != mm));
    CHECK_FALSE((mm != ci));
    CHECK_FALSE((mm != cm));
    CHECK_FALSE((mm != mi));
    CHECK_FALSE((mm != mm));
}
