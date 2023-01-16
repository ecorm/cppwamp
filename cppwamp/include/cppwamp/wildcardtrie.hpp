/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_WILDCARDTRIE_HPP
#define CPPWAMP_WILDCARDTRIE_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains the WildcardTrie template class. */
//------------------------------------------------------------------------------

#include <cassert>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include "api.hpp"
#include "uri.hpp"
#include "internal/wildcardtrienode.hpp"

namespace wamp
{

// Forward declaration
namespace internal { struct WildcardTrieIteratorAccess; }

//------------------------------------------------------------------------------
/** Detects if an iterator is one of the types returned by WildcardTrie. */
//------------------------------------------------------------------------------
template <typename I, typename Enable = void>
struct IsSpecialWildcardTrieIterator : std::false_type
{};

//------------------------------------------------------------------------------
/** WildcardTrie iterator that advances to wildcard matches in
    lexicographic order. */
//------------------------------------------------------------------------------
template <typename T, bool IsMutable>
class CPPWAMP_API WildcardTrieMatchIterator
{
public:
    /// The category of the iterator
    using iterator_category = std::forward_iterator_tag;

    /// Type used to identify distance between iterators
    using difference_type = std::ptrdiff_t;

    /// Type of the split URI associated with this iterator.
    using key_type = SplitUri;

    /// Type of the URI string associated with this iterator.
    using string_type = typename SplitUri::value_type;

    /** Type of the mapped value associated with this iterator.
        @note It differs from std::map in that it's not a key-value pair. */
    using value_type = T;

    /// Pointer to the mapped value type iterated over.
    using pointer = typename std::conditional<IsMutable, T*, const T*>::type;

    /// Reference to the mapped value type iterated over.
    using reference = typename std::conditional<IsMutable, T&, const T&>::type;

    /** Default constructor. */
    WildcardTrieMatchIterator();

    /** Implicit conversion from mutable iterator to const iterator. */
    template <bool RM, typename std::enable_if<!IsMutable && RM, int>::type = 0>
    WildcardTrieMatchIterator(const WildcardTrieMatchIterator<T, RM>& rhs);

    /** Assignment from mutable iterator to const iterator. */
    template <bool RM, typename std::enable_if<!IsMutable && RM, int>::type = 0>
    WildcardTrieMatchIterator&
    operator=(const WildcardTrieMatchIterator<T, RM>& rhs);

    /** Generates the split URI labels associated with the current element. */
    key_type key() const;

    /** Generates the URI associated with the current element. */
    string_type uri() const;

    /** Accesses the value associated with the current element. */
    reference value();

    /** Accesses the value associated with the current element. */
    const value_type& value() const;

    /** Accesses the value associated with the current element. */
    reference operator*();

    /** Accesses the value associated with the current element. */
    const value_type& operator*() const;

    /** Accesses a member of the value associated with the current element. */
    pointer operator->();

    /** Accesses a member of the value associated with the current element. */
    const value_type* operator->() const;

    /** Prefix increment, advances to the next matching key
        in lexigraphic order. */
    WildcardTrieMatchIterator& operator++();

    /** Postfix increment, advances to the next matching key
        in lexigraphic order. */
    WildcardTrieMatchIterator operator++(int);

private:
    using Access = internal::WildcardTrieIteratorAccess;
    using Cursor = internal::WildcardTrieCursor<value_type>;

    explicit WildcardTrieMatchIterator(Cursor endCursor);

    explicit WildcardTrieMatchIterator(Cursor beginCursor, SplitUri labels_);

    SplitUri key_;
    Cursor cursor_;
    typename SplitUri::size_type level_ = 0;

    template <typename> friend class WildcardTrie;
    friend struct internal::WildcardTrieIteratorAccess;
};

/** Compares two match iterators for equality.
    @relates WildcardTrieMatchIterator */
template <typename T, bool LM, bool RM>
bool operator==(const WildcardTrieMatchIterator<T, LM>& lhs,
                const WildcardTrieMatchIterator<T, RM>& rhs)
{
    return internal::WildcardTrieIteratorAccess::equals(lhs, rhs);
};

/** Compares two match iterators for inequality.
    @relates WildcardTrieMatchIterator */
template <typename T, bool LM, bool RM>
bool operator!=(const WildcardTrieMatchIterator<T, LM>& lhs,
                const WildcardTrieMatchIterator<T, RM>& rhs)
{
    return internal::WildcardTrieIteratorAccess::differs(lhs, rhs);
};

template <typename T, bool M>
struct IsSpecialWildcardTrieIterator<WildcardTrieMatchIterator<T, M>>
    : std::true_type
{};


//------------------------------------------------------------------------------
/** WildcardTrie iterator that advances to contained values in
    lexicographic order of their associated keys. */
//------------------------------------------------------------------------------
template <typename T, bool IsMutable>
class CPPWAMP_API WildcardTrieIterator
{
public:
    /// The category of the iterator.
    using iterator_category = std::forward_iterator_tag;

    /// Type used to identify distance between iterators.
    using difference_type = std::ptrdiff_t;

    /// Type of the split URI associated with this iterator.
    using key_type = SplitUri;

    /// Type of the URI string associated with this iterator.
    using string_type = typename SplitUri::value_type;

    /** Type of the mapped value associated with this iterator.
        @note It differs from std::map in that it's not a key-value pair. */
    using value_type = T;

    /// Pointer to the mapped value type iterated over.
    using pointer = typename std::conditional<IsMutable, T*, const T*>::type;

    /// Reference to the mapped value type iterated over.
    using reference = typename std::conditional<IsMutable, T&, const T&>::type;


    /** Default constructor. */
    WildcardTrieIterator();

    /** Implicit conversion from mutable iterator to const iterator. */
    template <bool M, typename std::enable_if<!IsMutable && M, int>::type = 0>
    WildcardTrieIterator(const WildcardTrieIterator<T, M>& rhs);

    /** Implicit conversion from match iterator. */
    template <bool M, typename std::enable_if<(!IsMutable || M), int>::type = 0>
    WildcardTrieIterator(const WildcardTrieMatchIterator<T, M>& rhs);

    /** Assignment from mutable iterator to const iterator. */
    template <bool M, typename std::enable_if<!IsMutable && M, int>::type = 0>
    WildcardTrieIterator& operator=(const WildcardTrieIterator<T, M>& rhs);

    /** Assignment from match iterator. */
    template <bool M, typename std::enable_if<(!IsMutable || M), int>::type = 0>
    WildcardTrieIterator& operator=(const WildcardTrieIterator<T, M>& rhs);

    /** Generates the split URI labels associated with the current element. */
    key_type key() const;

    /** Generates the URI associated with the current element. */
    string_type uri() const;

    /** Accesses the value associated with the current element. */
    reference value();

    /** Accesses the value associated with the current element. */
    const value_type& value() const;

    /** Accesses the value associated with the current element. */
    reference operator*();

    /** Accesses the value associated with the current element. */
    const value_type& operator*() const;

    /** Accesses a member of the value associated with the current element. */
    pointer operator->();

    /** Accesses a member of the value associated with the current element. */
    const value_type* operator->() const;

    /** Prefix increment, advances to the next key in lexigraphic order. */
    WildcardTrieIterator& operator++();

    /** Postfix increment, advances to the next key in lexigraphic order. */
    WildcardTrieIterator operator++(int);

private:
    using Access = internal::WildcardTrieIteratorAccess;
    using Cursor = internal::WildcardTrieCursor<value_type>;

    explicit WildcardTrieIterator(Cursor cursor);

    Cursor cursor_;

    template <typename> friend class WildcardTrie;
    friend struct internal::WildcardTrieIteratorAccess;
};

template <typename T, bool M>
struct IsSpecialWildcardTrieIterator<WildcardTrieIterator<T, M>>
    : std::true_type
{};

/** Compares two iterators for equality.
    @relates WildcardTrieIterator */
template <typename T, bool LM, bool RM>
bool operator==(const WildcardTrieIterator<T, LM>& lhs,
                const WildcardTrieIterator<T, RM>& rhs)
{
    return internal::WildcardTrieIteratorAccess::equals(lhs, rhs);
};

/** Compares two iterators for inequality.
    @relates WildcardTrieIterator */
template <typename T, bool LM, bool RM>
bool operator!=(const WildcardTrieIterator<T, LM>& lhs,
                const WildcardTrieIterator<T, RM>& rhs)
{
    return internal::WildcardTrieIteratorAccess::differs(lhs, rhs);
};

/** Compares a match iterator and a regular iterator for equality.
    @relates WildcardTrieIterator */
template <typename T, bool LM, bool RM>
bool operator==(const WildcardTrieMatchIterator<T, LM>& lhs,
                const WildcardTrieIterator<T, RM>& rhs)
{
    return internal::WildcardTrieIteratorAccess::equals(lhs, rhs);
};

/** Compares a match iterator and a regular iterator for equality.
    @relates WildcardTrieIterator */
template <typename T, bool LM, bool RM>
bool operator==(const WildcardTrieIterator<T, LM>& lhs,
                const WildcardTrieMatchIterator<T, RM>& rhs)
{
    return internal::WildcardTrieIteratorAccess::equals(lhs, rhs);
};

/** Compares a match iterator and a regular iterator for inequality.
    @relates WildcardTrieIterator */
template <typename T, bool LM, bool RM>
bool operator!=(const WildcardTrieMatchIterator<T, LM>& lhs,
                const WildcardTrieIterator<T, RM>& rhs)
{
    return internal::WildcardTrieIteratorAccess::differs(lhs, rhs);
};

/** Compares a match iterator and a regular iterator for inequality.
    @relates WildcardTrieIterator */
template <typename T, bool LM, bool RM>
bool operator!=(const WildcardTrieIterator<T, LM>& lhs,
                const WildcardTrieMatchIterator<T, RM>& rhs)
{
    return internal::WildcardTrieIteratorAccess::differs(lhs, rhs);
};


//------------------------------------------------------------------------------
/** Associative container that performs efficient searches of wildcard URI
    patterns matching a given URI.

    Like std::map, this container does not invalidate iterators during
    - insertions
    - erasures
    - swaps

    In addition, this container further guarantees that non-end iterators are
    not invalidated during
    - move-construction
    - move-assignment
    - self-move-assignment
    - self-copy-assignment
    - self-swap.

    @tparam T Type of the mapped value. Must be default constructible. */
//------------------------------------------------------------------------------
template <typename T>
class CPPWAMP_API WildcardTrie
{
private:
    using Tree = typename internal::WildcardTrieNode<T>::Tree;

    template <typename P>
    static constexpr bool isInsertable()
    {
        return std::is_constructible<value_type, P&&>::value;
    }

    template <typename I>
    static constexpr bool isSpecial()
    {
        return IsSpecialWildcardTrieIterator<I>::value;
    }

public:
    /** Split URI type used as the primary key type. */
    using key_type = SplitUri;

    /** URI string type that can be used as an alternate key type that is
        automatically split into labels. */
    using string_type = typename SplitUri::value_type;

    /** Type of the mapped value. */
    using mapped_type = T;

    /** Type used to combine a key and its associated value.
        @note Unlike std::map, keys are not stored alongside their associated
              value due to the distributed nature of how keys are stored inside
              the trie. This type is provided to make the interface more closely
              match that of std::map. */
    using value_type = std::pair<const key_type, mapped_type>;

    /** Type used to count the number of elements in the container. */
    using size_type = typename Tree::size_type;

    /** Mutable iterator type which advances through contained values in
        lexicographic order of their associated keys. */
    using iterator  = WildcardTrieIterator<T, true>;

    /** Immutable iterator type which advances through contained values in
        lexicographic order of their associated keys. */
    using const_iterator = WildcardTrieIterator<T, false>;

    /** Mutable iterator type which advances through wildcard matches in
        lexicographic order. */
    using match_iterator = WildcardTrieMatchIterator<T, true>;

    /** Mutable iterator type which advances through wildcard matches in
        lexicographic order. */
    using const_match_iterator = WildcardTrieMatchIterator<T, false>;

    /** Pairs an iterator with the boolean success result of an
        insertion operation. */
    using result = std::pair<iterator, bool>;

    /** Pairs two mutable iterators corresponding to the first and
        one-past-the-last match. */
    using match_range_type = std::pair<match_iterator, match_iterator>;

    /** Pairs two immutable iterators corresponding to the first and
        one-past-the-last match. */
    using const_match_range_type = std::pair<const_match_iterator,
                                             const_match_iterator>;

    /** Default constructor. */
    WildcardTrie();

    /** Copy constructor. */
    WildcardTrie(const WildcardTrie& rhs);

    /** Move constructor. */
    WildcardTrie(WildcardTrie&& rhs) noexcept;

    /** Constructs using the given iterator range. */
    template <typename I>
    WildcardTrie(I first, I last);

    /** Constructs using contents of the given initializer list, where
        each element is a key-value pair. */
    WildcardTrie(std::initializer_list<value_type> list);

    /** Copy assignment. */
    WildcardTrie& operator=(const WildcardTrie& rhs);

    /** Move assignment. */
    WildcardTrie& operator=(WildcardTrie&& rhs) noexcept;

    /** Replaces contents with that of the given initializer list,  where
        each element is a key-value pair. */
    WildcardTrie& operator=(std::initializer_list<value_type> list);

    /// @{
    /// Element Access

    /** Accesses the element associated with the given key,
        with bounds checking. */
    mapped_type& at(const key_type& key);

    /** Accesses the element associated with the given key,
        with bounds checking. */
    const mapped_type& at(const key_type& key) const;

    /** Accesses the element associated with the given URI string,
        with bounds checking. */
    mapped_type& at(const string_type& uri);

    /** Accesses the element associated with the given URI string,
        with bounds checking. */
    const mapped_type& at(const string_type& uri) const;

    /** Accesses or inserts an element with the given key. */
    mapped_type& operator[](const key_type& key);

    /** Accesses or inserts an element with the given key. */
    mapped_type& operator[](key_type&& key);

    /** Accesses or inserts an element with the given URI string. */
    mapped_type& operator[](const string_type& uri);
    /// @}

    /// @{
    /// Iterators

    /** Obtains an iterator to the beginning. */
    iterator begin() noexcept;

    /** Obtains an iterator to the beginning. */
    const_iterator begin() const noexcept;

    /** Obtains an iterator to the beginning. */
    const_iterator cbegin() const noexcept;

    /** Obtains an iterator to the end. */
    iterator end() noexcept;

    /** Obtains an iterator to the end. */
    const_iterator end() const noexcept;

    /** Obtains an iterator to the end. */
    const_iterator cend() const noexcept;
    /// @}


    /// @{
    /// Capacity

    /** Checks whether the container is empty. */
    bool empty() const noexcept;

    /** Obtains the number of elements. */
    size_type size() const noexcept;
    /// @}


    /// @{
    /// Modifiers

    /** Removes all elements. */
    void clear() noexcept;

    /** Inserts an element. */
    result insert(const value_type& kv);

    /** Inserts an element. */
    result insert(value_type&& kv);

    /** Inserts an element.
        Only participates in overload resolution if
        `std::is_constructible_v<value_type, P&&> == true` */
    template <typename P,
              typename std::enable_if<isInsertable<P>(), int>::type = 0>
    result insert(P&& kv)
    {
        value_type pair(std::forward<P>(kv));
        return add(std::move(pair.first), std::move(pair.second));
    }

    /** Inserts elements from the given iterator range, where the
        iterators dereference to a key-value pair. */
    template <typename I,
             typename std::enable_if<!isSpecial<I>(), int>::type = 0>
    void insert(I first, I last)
    {
        for (; first != last; ++first)
            add(first->first, first->second);
    }

    /** Inserts elements from the given iterator range, where the
        iterators provide key() and value() members. */
    template <typename I,
             typename std::enable_if<isSpecial<I>(), int>::type = 0>
    void insert(I first, I last)
    {
        for (; first != last; ++first)
            add(first.key(), first.value());
    }

    /** Inserts elements from the given initializer list of key-value pairs. */
    void insert(std::initializer_list<value_type> list);

    template <typename M>
    result insert_or_assign(const key_type& key, M&& arg);

    template <typename M>
    result insert_or_assign(key_type&& key, M&& arg);

    template <typename M>
    result insert_or_assign(const string_type& uri, M&& arg);

    template <typename... Us>
    result emplace(Us&&... args);

    template <typename... Us>
    result try_emplace(const key_type& key, Us&&... args);

    template <typename... Us>
    result try_emplace(key_type&& key, Us&&... args);

    template <typename... Us>
    result try_emplace(const string_type& uri, Us&&... args);

    iterator erase(iterator pos);

    iterator erase(const_iterator pos);

    size_type erase(const key_type& key);

    size_type erase(const string_type& uri);

    // Does not invalidate iterators, except the end iterator, as permitted
    // by the standard.
    void swap(WildcardTrie& other) noexcept;
    /// @}

    /// @{
    /// Lookup

    size_type count(const key_type& key) const;

    size_type count(const string_type& uri) const;

    iterator find(const key_type& key);

    const_iterator find(const key_type& key) const;

    iterator find(const string_type& uri);

    const_iterator find(const string_type& uri) const;

    bool contains(const key_type& key) const;

    bool contains(const string_type& uri) const;

    match_range_type match_range(const key_type& key);

    const_match_range_type match_range(const key_type& key) const;

    match_range_type match_range(const string_type& uri);

    const_match_range_type match_range(const string_type& uri) const;
    /// @}

private:
    using Node = internal::WildcardTrieNode<T>;
    using Cursor = internal::WildcardTrieCursor<T>;

    void moveFrom(WildcardTrie& rhs) noexcept;

    Cursor rootCursor();

    Cursor rootCursor() const;

    Cursor firstTerminalCursor();

    Cursor firstTerminalCursor() const;

    Cursor sentinelCursor();

    Cursor sentinelCursor() const;

    Cursor locate(const key_type& key);

    Cursor locate(const key_type& key) const;

    template <typename I>
    std::pair<I, I> getMatchRange(const key_type& key) const;

    template <typename... Us>
    std::pair<iterator, bool> add(key_type key, Us&&... args);

    template <typename... Us>
    std::pair<iterator, bool> put(bool clobber, key_type key, Us&&... args);

    void scanTree();

    Node sentinel_;
    std::unique_ptr<Node> root_;
    size_type size_ = 0;
};

template <typename T>
void swap(WildcardTrie<T>& a, WildcardTrie<T>& b) noexcept
{
    a.swap(b);
}

// TODO: Comparison operators


//******************************************************************************
// WildcardTrieMatchIterator implementation
//******************************************************************************

template <typename T, bool M>
WildcardTrieMatchIterator<T,M>::WildcardTrieMatchIterator() {}

template <typename T, bool M>
template <bool RM, typename std::enable_if<!M && RM, int>::type>
WildcardTrieMatchIterator<T,M>::WildcardTrieMatchIterator(
    const WildcardTrieMatchIterator<T, RM>& rhs)
    : cursor_(Access::cursor(rhs))
{}

template <typename T, bool M>
template <bool RM, typename std::enable_if<!M && RM, int>::type>
WildcardTrieMatchIterator<T,M>& WildcardTrieMatchIterator<T,M>::operator=(
    const WildcardTrieMatchIterator<T, RM>& rhs)
{
    cursor_ = Access::cursor(rhs);
    return *this;
}

template <typename T, bool M>
SplitUri WildcardTrieMatchIterator<T,M>::key() const
{
    return cursor_.generateKey();
}

template <typename T, bool M>
std::string WildcardTrieMatchIterator<T,M>::uri() const
{
    return untokenizeUri(key());
}

template <typename T, bool M>
typename WildcardTrieMatchIterator<T,M>::reference
WildcardTrieMatchIterator<T,M>::value()
{
    return cursor_.iter->second.value;
}

template <typename T, bool M>
const T& WildcardTrieMatchIterator<T,M>::value() const
{
    return cursor_.iter->second.value;
}

template <typename T, bool M>
typename WildcardTrieMatchIterator<T,M>::reference
WildcardTrieMatchIterator<T,M>::operator*()
{
    return value();
}

template <typename T, bool M>
const T& WildcardTrieMatchIterator<T,M>::operator*() const
{
    return value();
}

template <typename T, bool M>
typename WildcardTrieMatchIterator<T,M>::pointer
WildcardTrieMatchIterator<T,M>::operator->()
{
    return &(value());
}

template <typename T, bool M>
const T* WildcardTrieMatchIterator<T,M>::operator->() const
{
    return &(value());
}

template <typename T, bool M>
WildcardTrieMatchIterator<T,M>& WildcardTrieMatchIterator<T,M>::operator++()
{
    cursor_.matchNext(key_, level_);
    return *this;
}

template <typename T, bool M>
WildcardTrieMatchIterator<T,M> WildcardTrieMatchIterator<T,M>::operator++(int)
{
    auto temp = *this;
    ++(*this);
    return temp;
}

template <typename T, bool M>
WildcardTrieMatchIterator<T,M>::WildcardTrieMatchIterator(Cursor endCursor)
    : cursor_(endCursor)
{}

template <typename T, bool M>
WildcardTrieMatchIterator<T,M>::WildcardTrieMatchIterator(Cursor beginCursor,
                                                          SplitUri labels_)
    : key_(std::move(labels_)),
      cursor_(beginCursor)
{
    level_ = cursor_.matchFirst(key_);
}



//******************************************************************************
// WildcardTrieIterator implementation
//******************************************************************************

template <typename T, bool M>
WildcardTrieIterator<T,M>::WildcardTrieIterator() {}

template <typename T, bool M>
template <bool RM, typename std::enable_if<!M && RM, int>::type>
WildcardTrieIterator<T,M>::WildcardTrieIterator(
    const WildcardTrieIterator<T, RM>& rhs)
    : cursor_(Access::cursor(rhs))
{}

template <typename T, bool M>
template <bool RM, typename std::enable_if<(!M || RM), int>::type>
WildcardTrieIterator<T,M>::WildcardTrieIterator(
    const WildcardTrieMatchIterator<T, RM>& rhs)
    : cursor_(Access::cursor(rhs))
{}

template <typename T, bool M>
template <bool RM, typename std::enable_if<!M && RM, int>::type>
WildcardTrieIterator<T,M>&
WildcardTrieIterator<T,M>::operator=(const WildcardTrieIterator<T, RM>& rhs)
{
    cursor_ = rhs.cursor_;
    return *this;
}

template <typename T, bool M>
template <bool RM, typename std::enable_if<(!M || RM), int>::type>
WildcardTrieIterator<T,M>&
WildcardTrieIterator<T,M>::operator=(const WildcardTrieIterator<T, RM>& rhs)
{
    cursor_ = rhs.cursor_;
    return *this;
}

template <typename T, bool M>
SplitUri WildcardTrieIterator<T,M>::key() const
{
    return cursor_.generateKey();
}

template <typename T, bool M>
std::string WildcardTrieIterator<T,M>::uri() const
{
    return untokenizeUri(key());
}

template <typename T, bool M>
typename WildcardTrieIterator<T,M>::reference
WildcardTrieIterator<T,M>::value()
{
    return cursor_.iter->second.value;
}

template <typename T, bool M>
const T& WildcardTrieIterator<T,M>::value() const
{
    return cursor_.iter->second.value;
}

template <typename T, bool M>
typename WildcardTrieIterator<T,M>::reference
WildcardTrieIterator<T,M>::operator*()
{
    return value();
}

template <typename T, bool M>
const T& WildcardTrieIterator<T,M>::operator*() const
{
    return value();
}

template <typename T, bool M>
typename WildcardTrieIterator<T,M>::pointer
WildcardTrieIterator<T,M>::operator->()
{
    return &(value());
}

template <typename T, bool M>
const T* WildcardTrieIterator<T,M>::operator->() const
{
    return &(value());
}

template <typename T, bool M>
WildcardTrieIterator<T,M>& WildcardTrieIterator<T,M>::operator++()
{
    cursor_.advanceToNextTerminal();
    return *this;
}

template <typename T, bool M>
WildcardTrieIterator<T,M> WildcardTrieIterator<T,M>::operator++(int)
{
    auto temp = *this;
    ++(*this);
    return temp;
}

template <typename T, bool M>
WildcardTrieIterator<T,M>::WildcardTrieIterator(Cursor cursor)
    : cursor_(cursor)
{}


//******************************************************************************
// WildcardTrie implementation
//******************************************************************************

template <typename T>
WildcardTrie<T>::WildcardTrie() {}

template <typename T>
WildcardTrie<T>::WildcardTrie(const WildcardTrie& rhs)
    : size_(rhs.size_)
{
    if (rhs.root_)
    {
        root_.reset(new Node(*rhs.root_));
        root_->parent = &sentinel_;
        scanTree();
    }
}

/** The moved-from container is left in a default-constructed state,
    except during self-assignment where it is left in its current state. */
template <typename T>
WildcardTrie<T>::WildcardTrie(WildcardTrie&& rhs) noexcept {moveFrom(rhs);}

template <typename T>
template <typename I>
WildcardTrie<T>::WildcardTrie(I first, I last)
    : WildcardTrie()
{
    insert(first, last);
}

template <typename T>
WildcardTrie<T>::WildcardTrie(std::initializer_list<value_type> list)
    : WildcardTrie(list.begin(), list.end())
{}

/** @par Exception Safety
    Has no effect if an exception is thrown while copying elements. */
template <typename T>
WildcardTrie<T>& WildcardTrie<T>::operator=(const WildcardTrie& rhs)
{
    // Do nothing for self-assignment to enfore the invariant that
    // the RHS iterators remain valid.
    if (&rhs != this)
    {
        WildcardTrie temp(rhs);
        (*this) = std::move(temp);
    }
    return *this;
}

/** The moved-from container is left in a default-constructed state,
    except during self-assignment where it is left in its current state. */
template <typename T>
WildcardTrie<T>& WildcardTrie<T>::operator=(WildcardTrie&& rhs) noexcept
{
    // Do nothing for self-move-assignment to avoid invalidating iterators.
    if (&rhs != this)
        moveFrom(rhs);
    return *this;
}

/** @par Exception Safety
    Has no effect if an exception is thrown while assigning elements. */
template <typename T>
WildcardTrie<T>&
WildcardTrie<T>::operator=(std::initializer_list<value_type> list)
{
    WildcardTrie temp(list);
    *this = std::move(temp);
    return *this;
}

/** @throws std::out_of_range if the container does not have an element
            with the given key. */
template <typename T>
typename WildcardTrie<T>::mapped_type& WildcardTrie<T>::at(const key_type& key)
{
    auto cursor = locate(key);
    if (cursor.isSentinel())
        throw std::out_of_range("wamp::WildcardTrie::at key out of range");
    return cursor.iter->second.value;
}

/** @throws std::out_of_range if the container does not have an element
            with the given key. */
template <typename T>
const typename WildcardTrie<T>::mapped_type&
WildcardTrie<T>::at(const key_type& key) const
{
    auto cursor = locate(key);
    if (cursor.isSentinel())
        throw std::out_of_range("wamp::WildcardTrie::at key out of range");
    return cursor.iter->second.value;
}

/** @throws std::out_of_range if the container does not have an element
            with the given URI. */
template <typename T>
typename WildcardTrie<T>::mapped_type&
WildcardTrie<T>::at(const string_type& uri)
{
    return at(tokenizeUri(uri));
}

/** @throws std::out_of_range if the container does not have an element
            with the given URI. */
template <typename T>
const typename WildcardTrie<T>::mapped_type&
WildcardTrie<T>::at(const string_type& uri) const
{
    return at(tokenizeUri(uri));
}

/** @par Exception Safety
    Has no effect if an exception is thrown during insertion of a
    new element. */
template <typename T>
typename WildcardTrie<T>::mapped_type&
WildcardTrie<T>::operator[](const key_type& key)
{
    return *(add(key).first);
}

/** @par Exception Safety
    Has no effect if an exception is thrown during insertion of a
    new element. */
template <typename T>
typename WildcardTrie<T>::mapped_type&
WildcardTrie<T>::operator[](key_type&& key)
{
    return *(add(std::move(key)).first);
}

/** @par Exception Safety
    Has no effect if an exception is thrown during insertion of a
    new element. */
template <typename T>
typename WildcardTrie<T>::mapped_type&
WildcardTrie<T>::operator[](const string_type& uri)
{
    return this->operator[](tokenizeUri(uri));
}

template <typename T>
typename WildcardTrie<T>::iterator WildcardTrie<T>::begin() noexcept
{
    return iterator{firstTerminalCursor()};
}

template <typename T>
typename WildcardTrie<T>::const_iterator WildcardTrie<T>::begin() const noexcept
{
    return cbegin();
}

template <typename T>
typename WildcardTrie<T>::const_iterator
WildcardTrie<T>::cbegin() const noexcept
{
    return const_iterator{firstTerminalCursor()};
}

template <typename T>
typename WildcardTrie<T>::iterator WildcardTrie<T>::end() noexcept
{
    return iterator{sentinelCursor()};
}

template <typename T>
typename WildcardTrie<T>::const_iterator WildcardTrie<T>::end() const noexcept
{
    return cend();
}

template <typename T>
typename WildcardTrie<T>::const_iterator WildcardTrie<T>::cend() const noexcept
{
    return const_iterator{sentinelCursor()};
}

template <typename T>
bool WildcardTrie<T>::empty() const noexcept {return size_ == 0;}

template <typename T>
typename WildcardTrie<T>::size_type WildcardTrie<T>::size() const noexcept
{
    return size_;
}

template <typename T>
void WildcardTrie<T>::clear() noexcept
{
    if (root_)
        root_->children.clear();
    size_ = 0;
}

template <typename T>
typename WildcardTrie<T>::result WildcardTrie<T>::insert(const value_type& kv)
{
    return add(kv.first, kv.second);
}

template <typename T>
typename WildcardTrie<T>::result WildcardTrie<T>::insert(value_type&& kv)
{
    return add(std::move(kv.first), std::move(kv.second));
}

template <typename T>
void WildcardTrie<T>::insert(std::initializer_list<value_type> list)
{
    insert(list.begin(), list.end());
}

template <typename T>
template <typename M>
typename WildcardTrie<T>::result
WildcardTrie<T>::insert_or_assign(const key_type& key, M&& arg)
{
    return put(true, key, std::forward<M>(arg));
}

template <typename T>
template <typename M>
typename WildcardTrie<T>::result
WildcardTrie<T>::insert_or_assign(key_type&& key, M&& arg)
{
    return put(true, std::move(key), std::forward<M>(arg));
}

template <typename T>
template <typename M>
typename WildcardTrie<T>::result
WildcardTrie<T>::insert_or_assign(const string_type& uri, M&& arg)
{
    return insert_or_assign(tokenizeUri(uri), std::forward<M>(arg));
}

template <typename T>
template <typename... Us>
typename WildcardTrie<T>::result WildcardTrie<T>::emplace(Us&&... args)
{
    return insert(value_type(std::forward<Us>(args)...));
}

template <typename T>
template <typename... Us>
typename WildcardTrie<T>::result
WildcardTrie<T>::try_emplace(const key_type& key, Us&&... args)
{
    return add(key, std::forward<Us>(args)...);
}

template <typename T>
template <typename... Us>
typename WildcardTrie<T>::result
WildcardTrie<T>::try_emplace(key_type&& key, Us&&... args)
{
    return add(std::move(key), std::forward<Us>(args)...);
}

template <typename T>
template <typename... Us>
typename WildcardTrie<T>::result
WildcardTrie<T>::try_emplace(const string_type& uri, Us&&... args)
{
    return add(tokenizeUri(uri), std::forward<Us>(args)...);
}

template <typename T>
typename WildcardTrie<T>::iterator WildcardTrie<T>::erase(iterator pos)
{
    auto cursor = pos.cursor_;
    assert(!cursor.isSentinel());
    ++pos;
    cursor.eraseFromHere();
    --size_;
    return pos;
}

template <typename T>
typename WildcardTrie<T>::iterator WildcardTrie<T>::erase(const_iterator pos)
{
    auto cursor = pos.cursor_;
    assert(!cursor.isSentinel());
    ++pos;
    cursor.eraseFromHere();
    --size_;
    return pos;
}

template <typename T>
typename WildcardTrie<T>::size_type WildcardTrie<T>::erase(const key_type& key)
{
    auto cursor = locate(key);
    bool found = !cursor.isSentinel();
    if (found)
    {
        cursor.eraseFromHere();
        --size_;
    }
    return found ? 1 : 0;
}

template <typename T>
typename WildcardTrie<T>::size_type
WildcardTrie<T>::erase(const string_type& uri)
{
    return erase(tokenizeUri(uri));
}

template <typename T>
void WildcardTrie<T>::swap(WildcardTrie& other) noexcept
{
    root_.swap(other.root_);
    std::swap(size_, other.size_);
    if (root_)
        root_->parent = &sentinel_;
    if (other.root_)
        other.root_->parent = &other.sentinel_;
}

template <typename T>
typename WildcardTrie<T>::size_type
WildcardTrie<T>::count(const key_type& key) const
{
    return locate(key).isSentinel() ? 0 : 1;
}

template <typename T>
typename WildcardTrie<T>::size_type
WildcardTrie<T>::count(const string_type& uri) const
{
    return count(tokenizeUri(uri));
}

template <typename T>
typename WildcardTrie<T>::iterator
WildcardTrie<T>::find(const key_type& key)
{
    return iterator{locate(key)};
}

template <typename T>
typename WildcardTrie<T>::const_iterator
WildcardTrie<T>::find(const key_type& key) const
{
    return const_iterator{locate(key)};
}

template <typename T>
typename WildcardTrie<T>::iterator WildcardTrie<T>::find(const string_type& uri)
{
    return find(tokenizeUri(uri));
}

template <typename T>
typename WildcardTrie<T>::const_iterator
WildcardTrie<T>::find(const string_type& uri) const
{
    return find(tokenizeUri(uri));
}

template <typename T>
bool WildcardTrie<T>::contains(const key_type& key) const
{
    return find(key) != cend();
}

template <typename T>
bool WildcardTrie<T>::contains(const string_type& uri) const
{
    return contains(tokenizeUri(uri));
}

template <typename T>
typename WildcardTrie<T>::match_range_type
WildcardTrie<T>::match_range(const key_type& key)
{
    return getMatchRange<match_iterator>(key);
}

template <typename T>
typename WildcardTrie<T>::const_match_range_type
WildcardTrie<T>::match_range(const key_type& key) const
{
    auto& self = const_cast<WildcardTrie&>(*this);
    return self.template getMatchRange<const_match_iterator>(key);
}

template <typename T>
typename WildcardTrie<T>::match_range_type
WildcardTrie<T>::match_range(const string_type& uri)
{
    return match_range(tokenizeUri(uri));
}

template <typename T>
typename WildcardTrie<T>::const_match_range_type
WildcardTrie<T>::match_range(const string_type& uri) const
{
    return match_range(tokenizeUri(uri));
}

template <typename T>
void WildcardTrie<T>::moveFrom(WildcardTrie& rhs) noexcept
{
    root_.swap(rhs.root_);
    size_ = rhs.size_;
    rhs.size_ = 0;
    if (root_)
        root_->parent = &sentinel_;
}

template <typename T>
typename WildcardTrie<T>::Cursor WildcardTrie<T>::rootCursor()
{
    assert(root_ != nullptr);
    return Cursor::begin(*root_);
}

template <typename T>
typename WildcardTrie<T>::Cursor WildcardTrie<T>::rootCursor() const
{
    assert(root_ != nullptr);
    return Cursor::begin(const_cast<Node&>(*root_));
}

template <typename T>
typename WildcardTrie<T>::Cursor WildcardTrie<T>::firstTerminalCursor()
{
    if (empty())
        return sentinelCursor();
    auto cursor = rootCursor();
    cursor.advanceToFirstTerminal();
    return cursor;
}

template <typename T>
typename WildcardTrie<T>::Cursor WildcardTrie<T>::firstTerminalCursor() const
{
    return const_cast<WildcardTrie&>(*this).firstTerminalCursor();
}

template <typename T>
typename WildcardTrie<T>::Cursor WildcardTrie<T>::sentinelCursor()
{
    return Cursor::end(sentinel_);
}

template <typename T>
typename WildcardTrie<T>::Cursor WildcardTrie<T>::sentinelCursor() const
{
    return Cursor::end(const_cast<Node&>(sentinel_));
}

template <typename T>
typename WildcardTrie<T>::Cursor WildcardTrie<T>::locate(const key_type& key)
{
    if (empty() || key.empty())
        return sentinelCursor();
    auto cursor = rootCursor();
    cursor.locate(key);
    return cursor;
}

template <typename T>
typename WildcardTrie<T>::Cursor
WildcardTrie<T>::locate(const key_type& key) const
{
    return const_cast<WildcardTrie&>(*this).locate(key);
}

template <typename T>
template <typename I>
std::pair<I, I> WildcardTrie<T>::getMatchRange(const key_type& key) const
{
    if (empty() || key.empty())
        return {I{sentinelCursor()}, I{sentinelCursor()}};

    return {I{rootCursor(), key}, I{sentinelCursor()}};
}

template <typename T>
template <typename... Us>
typename WildcardTrie<T>::result WildcardTrie<T>::add(key_type key,
                                                      Us&&... args)
{
    return put(false, std::move(key), std::forward<Us>(args)...);
}

template <typename T>
template <typename... Us>
typename WildcardTrie<T>::result
WildcardTrie<T>::put(bool clobber, key_type key, Us&&... args)
{
    if (key.empty())
        return {end(), false};

    if (!root_)
    {
        root_.reset(new Node);
        root_->parent = &sentinel_;
    }

    auto cursor = rootCursor();
    bool placed = cursor.put(clobber, std::move(key),
                             std::forward<Us>(args)...);
    if (placed)
        ++size_;
    return {iterator{cursor}, placed};
}

template <typename T>
void WildcardTrie<T>::scanTree()
{
    root_->position = root_->children.end();
    Node* parent = root_.get();
    auto iter = root_->children.begin();
    while (!parent->isRoot())
    {
        if (iter != parent->children.end())
        {
            auto& node = iter->second;
            node.position = iter;
            node.parent = parent;

            if (!node.isLeaf())
            {
                auto& child = iter->second;
                parent = &child;
                iter = child.children.begin();
            }
            else
            {
                ++iter;
            }
        }
        else
        {
            iter = parent->position;
            parent = parent->parent;
            if (!parent->isRoot())
                ++iter;
        }
    }
}

} // namespace wamp

#endif // CPPWAMP_WILDCARDTRIE_HPP
