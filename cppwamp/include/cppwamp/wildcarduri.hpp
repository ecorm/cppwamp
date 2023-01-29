/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_WILDCARDURI_HPP
#define CPPWAMP_WILDCARDURI_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for processing URIs. */
//------------------------------------------------------------------------------

#include <algorithm>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include "api.hpp"
#include "erroror.hpp"
#include "tagtypes.hpp"
#include "tokentrie.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Contains a URI split into its constituent labels.

    Provides a subset of vector-like operations, as well as additional
    functions for converting from/to URI strings. To access the complete set
    of vector operations, use the SplitUri::labels accessor. */
//------------------------------------------------------------------------------
class CPPWAMP_API SplitUri
{
public:
    using uri_type         = std::string;
    using label_type       = uri_type;
    using char_type        = typename uri_type::value_type;
    using storage_type     = std::vector<std::string>;
    using value_type       = label_type;
    using allocator_type   = typename storage_type::allocator_type;
    using size_type        = typename storage_type::size_type;
    using difference_type  = typename storage_type::difference_type;
    using reference        = label_type&;
    using const_reference  = const label_type&;
    using pointer          = typename storage_type::pointer;
    using const_pointer    = typename storage_type::const_pointer;
    using iterator         = typename storage_type::iterator;
    using const_iterator   = typename storage_type::const_iterator;

    static constexpr char_type separator = '.';

    SplitUri()
        noexcept(std::is_nothrow_default_constructible<storage_type>::value) {}

    SplitUri(const uri_type& uri): labels_(tokenize(uri)) {}

    SplitUri(const char_type* uri): labels_(tokenize(uri)) {}

    SplitUri(const storage_type& labels): labels_(std::move(labels)) {}

    template <typename... Ts>
    SplitUri(in_place_t, Ts&&... args) : labels_(std::forward<Ts>(args)...) {}

    SplitUri& operator=(const uri_type& uri)
    {
        labels_ = tokenize(uri);
        return *this;
    }

    SplitUri& operator=(std::initializer_list<label_type> list)
    {
        labels_ = list;
        return *this;
    }

    /// @name Label Access
    /// @{
    reference at(size_type pos)                     {return labels_.at(pos);}
    const_reference at(size_type pos) const         {return labels_.at(pos);}
    reference operator[] (size_type pos)            {return labels_[pos];}
    const_reference operator[](size_type pos) const {return labels_[pos];}
    reference front()                               {return labels_.front();}
    const_reference front() const                   {return labels_.front();}
    reference back()                                {return labels_.back();}
    const_reference back() const                    {return labels_.back();}
    ///@}

    /// @name Iterators
    /// @{
    iterator begin() noexcept                       {return labels_.begin();}
    const_iterator begin() const noexcept           {return labels_.begin();}
    const_iterator cbegin() const noexcept          {return labels_.cbegin();}
    iterator end() noexcept                         {return labels_.end();}
    const_iterator end() const noexcept             {return labels_.end();}
    const_iterator cend() const noexcept            {return labels_.cend();}
    /// @}

    /// @name Capacity
    /// @{
    bool empty() const noexcept         {return labels_.empty();}
    size_type size() const noexcept     {return labels_.size();}
    size_type max_size() const noexcept {return labels_.max_size();}
    /// @}

    /// @name Modifiers
    /// @{
    void clear() noexcept               {labels_.clear();}
    void push_back(const label_type& s) {labels_.push_back(s);}
    void push_back(label_type&& s)      {labels_.push_back(std::move(s));}
    void swap(SplitUri& x)
        noexcept(std::is_nothrow_swappable<storage_type>::value)
        {labels_.swap(x.labels_);}
    /// @}

    /// @name Labels
    /// @{
    /** Return an URI string composed of this object's split labels. */
    ErrorOr<uri_type> flatten() const;

    /** Accesses the underlying container of split labels. */
    storage_type& labels() noexcept {return labels_;}

    /** Accesses the underlying container of split labels. */
    const storage_type& labels() const noexcept {return labels_;}
    /// @}

    /// @name Non-member functions
    /// @{
    friend bool operator==(const SplitUri& a, const SplitUri& b) {return a.labels_ == b.labels_;}
    friend bool operator!=(const SplitUri& a, const SplitUri& b) {return a.labels_ != b.labels_;}
    friend bool operator< (const SplitUri& a, const SplitUri& b) {return a.labels_ <  b.labels_;}
    friend bool operator<=(const SplitUri& a, const SplitUri& b) {return a.labels_ <= b.labels_;}
    friend bool operator> (const SplitUri& a, const SplitUri& b) {return a.labels_ >  b.labels_;}
    friend bool operator>=(const SplitUri& a, const SplitUri& b) {return a.labels_ >= b.labels_;}

    friend void swap(SplitUri& a, SplitUri& b) {a.swap(b);}

    friend std::ostream& operator<<(const SplitUri& x, std::ostream& out)
    {
        return out << x.flatten().value_or("<null>");
    }
    /// @}

private:
    static storage_type tokenize(const uri_type uri);

    static uri_type untokenize(const storage_type& labels);

    storage_type labels_;
};

//------------------------------------------------------------------------------
/** Obtains the URI wildcard label.
    @relates SplitUri */
//------------------------------------------------------------------------------
inline SplitUri::label_type CPPWAMP_API wildcardLabel()
{
    return SplitUri::label_type();
}

//------------------------------------------------------------------------------
/** Determines if the given uri label is a wildcard.
    @relates SplitUri */
//------------------------------------------------------------------------------
inline bool CPPWAMP_API isWildcardLabel(const SplitUri::label_type& label)
{
    return label.empty();
}

//------------------------------------------------------------------------------
/** Determines if the given SplitUri matches the given wildcard pattern.
    @relates SplitUri */
//------------------------------------------------------------------------------
bool CPPWAMP_API matchesWildcardPattern(const SplitUri& uri,
                                        const SplitUri& pattern);


//------------------------------------------------------------------------------
template <typename T>
using UriTrie = TokenTrie<SplitUri, T>;


//------------------------------------------------------------------------------
/** TokenTrie traverser that advances through wildcard matches in
    lexicographic order. */
//------------------------------------------------------------------------------
template <typename C>
class CPPWAMP_API WildcardMatcher
{
public:
    /// Type of the underlying cursor used to traverse nodes. */
    using Cursor = C;

    /// Type of the split token key container associated with this visitor.
    using Key = typename C::key_type;

    /** Type of the mapped value associated with this visitor. */
    using Value = typename C::mapped_type;

    /// Reference to the mapped value type being visited.
    using Reference = typename std::conditional<C::is_mutable(), Value&,
                                                const Value&>::type;

    /** Default constructor. */
    WildcardMatcher(Key key, Cursor root, Cursor sentinel);

    /** Generates the split token key container associated with the
        current element. */
    Key key() const {return cursor_.key();}

    /** Accesses the value associated with the current element. */
    Reference value() {return cursor_.value();}

    /** Accesses the value associated with the current element. */
    const Value& value() const {return cursor_.value();}

    /** Determines if there are remaining matching elements left. */
    explicit operator bool() const {return !done();}

    /** Determines if there are no more remaining matching elements left. */
    bool done() const {return cursor_.at_end();}

    /** Advances to the next matching key in lexigraphic order. */
    WildcardMatcher& next();

    /** Invokes the given functor for every matching key by passing it the
        key and corresponding value. */
    template <typename F>
    void forEach(F&& functor);

private:
    using Level = typename Key::size_type;
    using Token = typename Key::value_type;

    struct Less;

    bool isMatch() const;
    void matchNext();
    bool tokenMatches(const Token& expectedToken) const;
    void findNextMatchCandidate();
    void findTokenInLevel(const Token& token);

    Key key_;
    Cursor cursor_;
    Level level_ = 0;
};

//------------------------------------------------------------------------------
template <typename T>
WildcardMatcher<typename UriTrie<T>::cursor> CPPWAMP_API
wildcardMatches(UriTrie<T>& trie, const SplitUri& key)
{
    using Cursor = typename UriTrie<T>::cursor;
    return WildcardMatcher<Cursor>(key, trie.root(), trie.sentinel());
}

//------------------------------------------------------------------------------
template <typename T>
WildcardMatcher<typename UriTrie<T>::const_cursor> CPPWAMP_API
wildcardMatches(const UriTrie<T>& trie, const SplitUri& key)
{
    using Cursor = typename UriTrie<T>::const_cursor;
    return WildcardMatcher<Cursor>(key, trie.root(), trie.sentinel());
}


//******************************************************************************
// WildcardMatcher member definitions
//******************************************************************************

//------------------------------------------------------------------------------
template <typename C>
struct WildcardMatcher<C>::Less
{
    template <typename KV>
    bool operator()(const KV& kv, const Token& s) const
    {
        return kv.first < s;
    }

    template <typename KV>
    bool operator()(const Token& s, const KV& kv) const
    {
        return s < kv.first;
    }
};

//------------------------------------------------------------------------------
template <typename C>
WildcardMatcher<C>::WildcardMatcher(Key key, Cursor root, Cursor sentinel)
    : key_(std::move(key)),
    cursor_(root)
{
    if (key_.empty())
        cursor_ = sentinel;
    else if (!isMatch())
        matchNext();
}

//------------------------------------------------------------------------------
template <typename C>
WildcardMatcher<C>& WildcardMatcher<C>::next()
{
    assert(!done());
    matchNext();
    return *this;
}

//------------------------------------------------------------------------------
template <typename C>
template <typename F>
void WildcardMatcher<C>::forEach(F&& functor)
{
    while (!done())
    {
        std::forward<F>(functor)(key(), value());
        next();
    }
}

//------------------------------------------------------------------------------
template <typename C>
bool WildcardMatcher<C>::isMatch() const
{
    assert(!key_.empty());
    const Level maxLevel = key_.size() - 1;
    if ((level_ != maxLevel) || cursor_.at_end_of_level())
        return false;

    // All nodes above the current level are matches. Only the bottom
    // level needs to be checked.
    assert(level_ < key_.size());
    return cursor_.has_value() && tokenMatches(key_[level_]);
}

//------------------------------------------------------------------------------
template <typename C>
void WildcardMatcher<C>::matchNext()
{
    while (!cursor_.at_end())
    {
        findNextMatchCandidate();
        if (isMatch())
            break;
    }
}

//------------------------------------------------------------------------------
template <typename C>
bool WildcardMatcher<C>::tokenMatches(const Token& expectedToken) const
{
    return cursor_.token().empty() || cursor_.token() == expectedToken;
}

//------------------------------------------------------------------------------
template <typename C>
void WildcardMatcher<C>::findNextMatchCandidate()
{
    const Level maxLevel = key_.size() - 1;
    if (!cursor_.at_end_of_level())
    {
        assert(level_ <=  maxLevel);
        const auto& expectedToken = key_[level_];
        bool canDescend = !cursor_.child()->is_leaf() &&
                          (level_ < maxLevel) &&
                          tokenMatches(expectedToken);
        if (canDescend)
            level_ = cursor_.descend(level_);
        else
            findTokenInLevel(expectedToken);
    }
    else if (!cursor_.at_end())
    {
        level_ = cursor_.ascend(level_);
        if (!cursor_.at_end_of_level())
            findTokenInLevel(key_[level_]);
    }
}

//------------------------------------------------------------------------------
template <typename C>
void WildcardMatcher<C>::findTokenInLevel(const Token& token)
{
    auto iter = cursor_.iter();
    auto end = cursor_.end();
    if (iter == cursor_.begin())
    {
        if (token.empty())
        {
            iter = end;
        }
        else
        {
            iter = cursor_.lower_bound(token);
            if (iter != end && iter->first != token)
                iter = end;
        }
    }
    else
    {
        iter = end;
    }
    cursor_.skip_to(iter);
}

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "./internal/wildcarduri.ipp"
#endif

#endif // CPPWAMP_WILDCARDURI_HPP
