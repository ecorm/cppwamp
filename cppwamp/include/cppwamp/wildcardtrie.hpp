/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_WILDCARDTRIE_HPP
#define CPPWAMP_WILDCARDTRIE_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains the TokenTrie template class. */
//------------------------------------------------------------------------------

#include <cassert>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include "api.hpp"
#include "wildcarduri.hpp"
#include "internal/wildcardtrienode.hpp"

namespace wamp
{

// Forward declaration
namespace internal { struct TokenTrieIteratorAccess; }

//------------------------------------------------------------------------------
/** Detects if an iterator is one of the types returned by TokenTrie. */
//------------------------------------------------------------------------------
template <typename I, typename Enable = void>
struct IsSpecialTokenTrieIterator : std::false_type
{};

//------------------------------------------------------------------------------
/** TokenTrie iterator that advances through wildcard matches in
    lexicographic order. */
//------------------------------------------------------------------------------
template <typename K, typename T, bool IsMutable>
class CPPWAMP_API TokenTrieMatchIterator
{
public:
    /// The category of the iterator
    using iterator_category = std::forward_iterator_tag;

    /// Type used to identify distance between iterators
    using difference_type = std::ptrdiff_t;

    /// Type of the key associated with this iterator.
    using key_type = K;

    /// Type of the URI string associated with this iterator.
    using uri_type = typename SplitUri::value_type;

    /** Type of the mapped value associated with this iterator.
        @note It differs from std::map in that it's not a key-value pair. */
    using value_type = T;

    /// Pointer to the mapped value type being iterated over.
    using pointer = typename std::conditional<IsMutable, T*, const T*>::type;

    /// Reference to the mapped value type being iterated over.
    using reference = typename std::conditional<IsMutable, T&, const T&>::type;

    /** Default constructor. */
    TokenTrieMatchIterator() {}

    /** Implicit conversion from mutable iterator to const iterator. */
    template <bool RM, typename std::enable_if<!IsMutable && RM, int>::type = 0>
    TokenTrieMatchIterator(const TokenTrieMatchIterator<K, T, RM>& rhs)
        : cursor_(Access::cursor(rhs)) {}

    /** Assignment from mutable iterator to const iterator. */
    template <bool RM, typename std::enable_if<!IsMutable && RM, int>::type = 0>
    TokenTrieMatchIterator&
    operator=(const TokenTrieMatchIterator<K, T, RM>& rhs)
        {cursor_ = Access::cursor(rhs); return *this;}

    /** Generates the split URI labels associated with the current element. */
    key_type key() const {return cursor_.generateKey();}

    /** Obtains the token associated with the current element. */
    uri_type token() const {return cursor_.token();}

    /** Accesses the value associated with the current element. */
    reference value() {return cursor_.iter->second.value;}

    /** Accesses the value associated with the current element. */
    const value_type& value() const {return cursor_.iter->second.value;}

    /** Accesses the value associated with the current element. */
    reference operator*() {return value();}

    /** Accesses the value associated with the current element. */
    const value_type& operator*() const {return value();}

    /** Accesses a member of the value associated with the current element. */
    pointer operator->() {return &(value());}

    /** Accesses a member of the value associated with the current element. */
    const value_type* operator->() const {return &(value());}

    /** Prefix increment, advances to the next matching key
        in lexigraphic order. */
    TokenTrieMatchIterator& operator++()
        {cursor_.matchNext(key_, level_); return *this;}

    /** Postfix increment, advances to the next matching key
        in lexigraphic order. */
    TokenTrieMatchIterator operator++(int)
        {auto temp = *this; ++(*this); return temp;}

private:
    using Access = internal::TokenTrieIteratorAccess;
    using Cursor = internal::TokenTrieCursor<value_type>;

    TokenTrieMatchIterator(Cursor endCursor) : cursor_(endCursor) {}

    TokenTrieMatchIterator(Cursor beginCursor, SplitUri labels_)
        : key_(std::move(labels_)), cursor_(beginCursor)
    {
        level_ = cursor_.matchFirst(key_);
    }


    SplitUri key_;
    Cursor cursor_;
    typename SplitUri::size_type level_ = 0;

    template <typename> friend class TokenTrie;
    friend struct internal::TokenTrieIteratorAccess;
};

/** Compares two match iterators for equality.
    @relates TokenTrieMatchIterator */
template <typename T, typename K, bool LM, bool RM>
bool operator==(const TokenTrieMatchIterator<K, T, LM>& lhs,
                const TokenTrieMatchIterator<K, T, RM>& rhs)
{
    return internal::TokenTrieIteratorAccess::equals(lhs, rhs);
};

/** Compares two match iterators for inequality.
    @relates TokenTrieMatchIterator */
template <typename T, typename K, bool LM, bool RM>
bool operator!=(const TokenTrieMatchIterator<K, T, LM>& lhs,
                const TokenTrieMatchIterator<K, T, RM>& rhs)
{
    return internal::TokenTrieIteratorAccess::differs(lhs, rhs);
};

template <typename T, typename K, bool M>
struct IsSpecialTokenTrieIterator<TokenTrieMatchIterator<K, T, M>>
    : std::true_type
{};


//------------------------------------------------------------------------------
/** TokenTrie iterator that advances through elements in lexicographic order
    of their respective keys. */
//------------------------------------------------------------------------------
template <typename K, typename T, bool IsMutable>
class CPPWAMP_API TokenTrieIterator
{
public:
    /// The category of the iterator.
    using iterator_category = std::forward_iterator_tag;

    /// Type used to identify distance between iterators.
    using difference_type = std::ptrdiff_t;

    /// Type of the split URI associated with this iterator.
    using key_type = K;

    /// Type of the URI string associated with this iterator.
    using label_type = typename K::value_type;

    /** Type of the mapped value associated with this iterator.
        @note It differs from std::map in that it's not a key-value pair. */
    using value_type = T;

    /// Pointer to the mapped value type being iterated over.
    using pointer = typename std::conditional<IsMutable, T*, const T*>::type;

    /// Reference to the mapped value type being iterated over.
    using reference = typename std::conditional<IsMutable, T&, const T&>::type;


    /** Default constructor. */
    TokenTrieIterator() {}

    /** Implicit conversion from mutable iterator to const iterator. */
    template <bool M, typename std::enable_if<!IsMutable && M, int>::type = 0>
    TokenTrieIterator(const TokenTrieIterator<K, T, M>& rhs)
        : cursor_(Access::cursor(rhs)) {}

    /** Implicit conversion from match iterator. */
    template <bool M, typename std::enable_if<(!IsMutable || M), int>::type = 0>
    TokenTrieIterator(const TokenTrieMatchIterator<K, T, M>& rhs)
        : cursor_(Access::cursor(rhs)) {}

    /** Assignment from mutable iterator to const iterator. */
    template <bool M, typename std::enable_if<!IsMutable && M, int>::type = 0>
    TokenTrieIterator& operator=(const TokenTrieIterator<K, T, M>& rhs)
        {cursor_ = rhs.cursor_; return *this;}

    /** Assignment from match iterator. */
    template <bool M, typename std::enable_if<(!IsMutable || M), int>::type = 0>
    TokenTrieIterator& operator=(const TokenTrieIterator<K, T, M>& rhs)
        {cursor_ = rhs.cursor_; return *this;}

    /** Generates the split URI labels associated with the current element. */
    key_type key() const {return cursor_.generateKey();}

    /** Obtains the token associated with the current element. */
    label_type token() const {return cursor_.token();}

    /** Accesses the value associated with the current element. */
    reference value() {return cursor_.iter->second.value;}

    /** Accesses the value associated with the current element. */
    const value_type& value() const {return cursor_.iter->second.value;}

    /** Accesses the value associated with the current element. */
    reference operator*() {return value();}

    /** Accesses the value associated with the current element. */
    const value_type& operator*() const {return value();}

    /** Accesses a member of the value associated with the current element. */
    pointer operator->() {return &(value());}

    /** Accesses a member of the value associated with the current element. */
    const value_type* operator->() const {return &(value());}

    /** Prefix increment, advances to the next key in lexigraphic order. */
    TokenTrieIterator& operator++()
        {cursor_.advanceToNextTerminal(); return *this;}

    /** Postfix increment, advances to the next key in lexigraphic order. */
    TokenTrieIterator operator++(int)
        {auto temp = *this; ++(*this); return temp;}

private:
    using Access = internal::TokenTrieIteratorAccess;
    using Cursor = internal::TokenTrieCursor<value_type>;

    TokenTrieIterator(Cursor cursor) : cursor_(cursor) {}

    Cursor cursor_;

    template <typename> friend class TokenTrie;
    friend struct internal::TokenTrieIteratorAccess;
};

template <typename T, typename K, bool M>
struct IsSpecialTokenTrieIterator<TokenTrieIterator<K, T, M>>
    : std::true_type
{};

/** Compares two iterators for equality.
    @relates TokenTrieIterator */
template <typename T, typename K, bool LM, bool RM>
bool operator==(const TokenTrieIterator<K, T, LM>& lhs,
                const TokenTrieIterator<K, T, RM>& rhs)
{
    return internal::TokenTrieIteratorAccess::equals(lhs, rhs);
};

/** Compares two iterators for inequality.
    @relates TokenTrieIterator */
template <typename T, typename K, bool LM, bool RM>
bool operator!=(const TokenTrieIterator<K, T, LM>& lhs,
                const TokenTrieIterator<K, T, RM>& rhs)
{
    return internal::TokenTrieIteratorAccess::differs(lhs, rhs);
};

/** Compares a match iterator and a regular iterator for equality.
    @relates TokenTrieIterator */
template <typename T, typename K, bool LM, bool RM>
bool operator==(const TokenTrieMatchIterator<K, T, LM>& lhs,
                const TokenTrieIterator<K, T, RM>& rhs)
{
    return internal::TokenTrieIteratorAccess::equals(lhs, rhs);
};

/** Compares a match iterator and a regular iterator for equality.
    @relates TokenTrieIterator */
template <typename T, typename K, bool LM, bool RM>
bool operator==(const TokenTrieIterator<K, T, LM>& lhs,
                const TokenTrieMatchIterator<K, T, RM>& rhs)
{
    return internal::TokenTrieIteratorAccess::equals(lhs, rhs);
};

/** Compares a match iterator and a regular iterator for inequality.
    @relates TokenTrieIterator */
template <typename T, typename K, bool LM, bool RM>
bool operator!=(const TokenTrieMatchIterator<K, T, LM>& lhs,
                const TokenTrieIterator<K, T, RM>& rhs)
{
    return internal::TokenTrieIteratorAccess::differs(lhs, rhs);
};

/** Compares a match iterator and a regular iterator for inequality.
    @relates TokenTrieIterator */
template <typename T, typename K, bool LM, bool RM>
bool operator!=(const TokenTrieIterator<K, T, LM>& lhs,
                const TokenTrieMatchIterator<K, T, RM>& rhs)
{
    return internal::TokenTrieIteratorAccess::differs(lhs, rhs);
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
class CPPWAMP_API TokenTrie
{
private:
    using Node = internal::TokenTrieNode<T>;
    using NodeAllocatorTraits = std::allocator_traits<typename Node::Allocator>;

    template <typename P>
    static constexpr bool isInsertable()
    {
        return std::is_constructible<value_type, P&&>::value;
    }

    template <typename I>
    static constexpr bool isSpecial()
    {
        return IsSpecialTokenTrieIterator<I>::value;
    }

public:
    /** Split URI type used as the primary key type. */
    using key_type = SplitUri;

    /** Type of the mapped value. */
    using mapped_type = T;

    /** Type used to combine a key and its associated value.
        @note Unlike std::map, keys are not stored alongside their associated
              value due to the distributed nature of how keys are stored inside
              the trie. This type is provided to make the interface more closely
              match that of std::map. */
    using value_type = std::pair<const key_type, mapped_type>;

    /** Type used to count the number of elements in the container. */
    using size_type = typename Node::Size;

    /// Type used to identify distance between iterators.
    using difference_type = std::ptrdiff_t;

    /// Comparison function that determines how keys are sorted.
    using key_compare = std::less<key_type>;

    /// Allocator type
    using allocator_type =
        typename NodeAllocatorTraits::template rebind_alloc<value_type>;

    /// Reference to value_type.
    using reference = value_type&;

    /// Reference to constant value_type.
    using const_reference = const value_type&;

    /// Pointer to value_type
    using pointer = typename std::allocator_traits<allocator_type>::pointer;

    /// Pointer to const value_type
    using const_pointer =
        typename std::allocator_traits<allocator_type>::const_pointer;

    /** Mutable iterator type which advances through elements in lexicographic
        order of their respective keys. */
    using iterator  = TokenTrieIterator<key_type, T, true>;

    /** Immutable iterator type which advances through elements in
        lexicographic order of their respective keys. */
    using const_iterator = TokenTrieIterator<key_type, T, false>;

    /** Mutable iterator type which advances through wildcard matches in
        lexicographic order. */
    using match_iterator = TokenTrieMatchIterator<key_type, T, true>;

    /** Mutable iterator type which advances through wildcard matches in
        lexicographic order. */
    using const_match_iterator = TokenTrieMatchIterator<key_type, T, false>;

    /** Pairs an iterator with the boolean success result of an
        insertion operation. */
    using result = std::pair<iterator, bool>;

    /** Pair of mutable iterators corresponding to a range. */
    using range_type = std::pair<iterator, iterator>;

    /** Pair of immutable iterators corresponding to a range. */
    using const_range_type = std::pair<const_iterator, const_iterator>;

    /** Pair of mutable iterators corresponding to the first and
        one-past-the-last match. */
    using match_range_type = std::pair<match_iterator, match_iterator>;

    /** Pair of immutable iterators corresponding to the first and
        one-past-the-last match. */
    using const_match_range_type = std::pair<const_match_iterator,
                                             const_match_iterator>;

    class value_compare
    {
    public:
        using result_type = bool;
        using first_argument_type = value_type;
        using second_argument_type = value_type;

        value_compare() = default;

        bool operator()(const value_type& a, const value_type& b)
        {
            return comp(a.first, b.first);
        }

    protected:
        value_compare(key_compare c) : comp(std::move(c)) {}

        key_compare comp;
    };

    /** Default constructor. */
    TokenTrie() {}

    /** Copy constructor. */
    TokenTrie(const TokenTrie& rhs);

    /** Move constructor. */
    TokenTrie(TokenTrie&& rhs) noexcept;

    /** Constructs using the given iterator range. */
    template <typename I>
    TokenTrie(I first, I last) {insert(first, last);}

    /** Constructs using contents of the given initializer list, where
        each element is a key-value pair. */
    TokenTrie(std::initializer_list<value_type> list)
        : TokenTrie(list.begin(), list.end())
    {}

    /** Copy assignment. */
    TokenTrie& operator=(const TokenTrie& rhs);

    /** Move assignment. */
    TokenTrie& operator=(TokenTrie&& rhs) noexcept;

    /** Replaces contents with that of the given initializer list,  where
        each element is a key-value pair. */
    TokenTrie& operator=(std::initializer_list<value_type> list);

    allocator_type get_allocator() const noexcept {return allocator_type();}

    /// @name Element Access
    /// @{

    /** Accesses the element associated with the given key,
        with bounds checking. */
    mapped_type& at(const key_type& key);

    /** Accesses the element associated with the given key,
        with bounds checking. */
    const mapped_type& at(const key_type& key) const;

    /** Accesses or inserts an element with the given key. */
    mapped_type& operator[](const key_type& key);

    /** Accesses or inserts an element with the given key. */
    mapped_type& operator[](key_type&& key);

    /// @name Iterators
    /// @{

    /** Obtains an iterator to the beginning. */
    iterator begin() noexcept {return firstTerminalCursor();}

    /** Obtains an iterator to the beginning. */
    const_iterator begin() const noexcept {return cbegin();}

    /** Obtains an iterator to the beginning. */
    const_iterator cbegin() const noexcept {return firstTerminalCursor();}

    /** Obtains an iterator to the end. */
    iterator end() noexcept {return sentinelCursor();}

    /** Obtains an iterator to the end. */
    const_iterator end() const noexcept {return cend();}

    /** Obtains an iterator to the end. */
    const_iterator cend() const noexcept {return sentinelCursor();}
    /// @}


    /// @name Capacity
    /// @{

    /** Checks whether the container is empty. */
    bool empty() const noexcept {return size_ == 0;}

    /** Obtains the number of elements. */
    size_type size() const noexcept {return size_;}

    /** Obtains the maximum possible number of elements. */
    size_type max_size() const noexcept;
    /// @}


    /// @name Modifiers
    /// @{

    /** Removes all elements. */
    void clear() noexcept;

    /** Inserts an element. */
    result insert(const value_type& kv);

    /** Inserts an element. */
    result insert(value_type&& kv);

    /** Inserts an element.
        Only participates in overload resolution if
        `std::is_constructible_v<value_type, P&&> == true`
        @par Exception Safety
        Has no effect if an exception is thrown during insertion of the
        elements.*/
    template <typename P,
              typename std::enable_if<isInsertable<P>(), int>::type = 0>
    result insert(P&& kv)
    {
        value_type pair(std::forward<P>(kv));
        return add(std::move(pair.first), std::move(pair.second));
    }

    /** Inserts elements from the given iterator range. */
    template <typename I>
    void insert(I first, I last);

    /** Inserts elements from the given initializer list of key-value pairs. */
    void insert(std::initializer_list<value_type> list);

    /** Inserts an element or assigns to the current element if the key
        already exists. */
    template <typename M>
    result insert_or_assign(const key_type& key, M&& arg);

    /** Inserts an element or assigns to the current element if the key
        already exists. */
    template <typename M>
    result insert_or_assign(key_type&& key, M&& arg);

    /** Inserts an element from a key-value pair constructed in-place using
        the given arguments. */
    template <typename... Us>
    result emplace(Us&&... args);

    /** Inserts in-place only if the key does not exist. */
    template <typename... Us>
    result try_emplace(const key_type& key, Us&&... args);

    /** Inserts in-place only if the key does not exist. */
    template <typename... Us>
    result try_emplace(key_type&& key, Us&&... args);

    /** Erases the element at the given iterator position. */
    iterator erase(iterator pos);

    /** Erases the element at the given iterator position. */
    iterator erase(const_iterator pos);

    /** Erases the element associated with the given key. */
    size_type erase(const key_type& key);

    /** Swaps the contents of this container with the given container. */
    void swap(TokenTrie& other) noexcept;
    /// @}

    /// @name Lookup
    /// @{

    /** Returns the number of elements associated with the given key. */
    size_type count(const key_type& key) const
        {return locate(key).isSentinel() ? 0 : 1;}

    /** Finds the element associated with the given key. */
    iterator find(const key_type& key) {return locate(key);}

    /** Finds the element associated with the given key. */
    const_iterator find(const key_type& key) const {return locate(key);}

    /** Checks if the container contains the element with the given key. */
    bool contains(const key_type& key) const {return !locate(key).isSentinel();}

    /** Obtains the range of elements lexicographically matching
        the given key.*/
    range_type equal_range(const key_type& key)
        {return getEqualRange<iterator>(key);}

    /** Obtains the range of elements lexicographically matching
        the given key.*/
    const_range_type equal_range(const key_type& key) const
        {return getEqualRange<const_iterator>(key);}

    /** Obtains an iterator to the first element not less than the
        given key. */
    iterator lower_bound(const key_type& key) {return findLowerBound(key);}

    /** Obtains an iterator to the first element not less than the
        given key. */
    const_iterator lower_bound(const key_type& key) const
        {return findLowerBound(key);}

    /** Obtains an iterator to the first element greater than than the
        given key. */
    iterator upper_bound(const key_type& key) {return findUpperBound(key);}

    /** Obtains an iterator to the first element greater than than the
        given key. */
    const_iterator upper_bound(const key_type& key) const
        {return findUpperBound(key);}

    /** Obtains the range of elements with wildcard patterns matching
        the given key. */
    match_range_type match_range(const key_type& key)
        {return getMatchRange<match_iterator>(key);}

    /** Obtains the range of elements with wildcard patterns matching
        the given key. */
    const_match_range_type match_range(const key_type& key) const
        {return getMatchRange<const_match_iterator>(key);}

    /** Obtains the function that compares keys. */
    key_compare key_comp() const {return key_compare();}

    /** Obtains the function that compares keys in value_type objects. */
    value_compare value_comp() const {return value_compare();}

    /// @}

    /** Equality comparison. */
    friend bool operator==(const TokenTrie& a, const TokenTrie& b) noexcept
        {return equals(a, b);}

    /** Inequality comparison. */
    friend bool operator!=(const TokenTrie& a, const TokenTrie& b) noexcept
        {return differs(a, b);}

    /** Non-member swap. */
    friend void swap(TokenTrie& a, TokenTrie& b) noexcept {a.swap(b);}

    /** Erases all elements satisfying given criteria. */
    template <typename P>
    friend size_type erase_if(TokenTrie& t, P predicate)
        {return t.doEraseIf(std::move(predicate));}

private:
    using Cursor = internal::TokenTrieCursor<T>;

    static bool equals(const TokenTrie& a, const TokenTrie& b) noexcept;
    static bool differs(const TokenTrie& a, const TokenTrie& b) noexcept;
    void moveFrom(TokenTrie& rhs) noexcept;
    Cursor rootCursor();
    Cursor rootCursor() const;
    Cursor firstTerminalCursor();
    Cursor firstTerminalCursor() const;
    Cursor sentinelCursor();
    Cursor sentinelCursor() const;
    Cursor locate(const key_type& key);
    Cursor locate(const key_type& key) const;
    Cursor findLowerBound(const key_type& key) const;
    Cursor findUpperBound(const key_type& key) const;

    template <typename I>
    std::pair<I, I> getEqualRange(const key_type& key) const;

    template <typename I>
    std::pair<I, I> getMatchRange(const key_type& key) const;

    template <typename I>
    void insertRange(std::true_type, I first, I last);

    template <typename I>
    void insertRange(std::false_type, I first, I last);

    template <typename... Us>
    std::pair<iterator, bool> add(key_type key, Us&&... args);

    template <typename... Us>
    std::pair<iterator, bool> put(bool clobber, key_type key, Us&&... args);

    template <typename I>
    I eraseAt(I pos);

    void scanTree();

    template <typename P>
    size_type doEraseIf(P predicate);

    Node sentinel_;
    std::unique_ptr<Node> root_;
    size_type size_ = 0;
};


//******************************************************************************
// TokenTrie member function definitions
//******************************************************************************

template <typename T>
TokenTrie<T>::TokenTrie(const TokenTrie& rhs)
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
TokenTrie<T>::TokenTrie(TokenTrie&& rhs) noexcept {moveFrom(rhs);}

/** @par Exception Safety
    Has no effect if an exception is thrown while copying elements. */
template <typename T>
TokenTrie<T>& TokenTrie<T>::operator=(const TokenTrie& rhs)
{
    // Do nothing for self-assignment to enfore the invariant that
    // the RHS iterators remain valid.
    if (&rhs != this)
    {
        TokenTrie temp(rhs);
        (*this) = std::move(temp);
    }
    return *this;
}

/** The moved-from container is left in a default-constructed state,
    except during self-assignment where it is left in its current state. */
template <typename T>
TokenTrie<T>& TokenTrie<T>::operator=(TokenTrie&& rhs) noexcept
{
    // Do nothing for self-move-assignment to avoid invalidating iterators.
    if (&rhs != this)
        moveFrom(rhs);
    return *this;
}

/** @par Exception Safety
    Has no effect if an exception is thrown while assigning elements. */
template <typename T>
TokenTrie<T>&
TokenTrie<T>::operator=(std::initializer_list<value_type> list)
{
    TokenTrie temp(list);
    *this = std::move(temp);
    return *this;
}

/** @throws std::out_of_range if the container does not have an element
            with the given key. */
template <typename T>
typename TokenTrie<T>::mapped_type& TokenTrie<T>::at(const key_type& key)
{
    auto cursor = locate(key);
    if (cursor.isSentinel())
        throw std::out_of_range("wamp::TokenTrie::at key out of range");
    return cursor.iter->second.value;
}

/** @throws std::out_of_range if the container does not have an element
            with the given key. */
template <typename T>
const typename TokenTrie<T>::mapped_type&
TokenTrie<T>::at(const key_type& key) const
{
    auto cursor = locate(key);
    if (cursor.isSentinel())
        throw std::out_of_range("wamp::TokenTrie::at key out of range");
    return cursor.iter->second.value;
}

/** @par Exception Safety
    Has no effect if an exception is thrown during insertion of a
    new element. */
template <typename T>
typename TokenTrie<T>::mapped_type&
TokenTrie<T>::operator[](const key_type& key)
{
    return *(add(key).first);
}

/** @par Exception Safety
    Has no effect if an exception is thrown during insertion of a
    new element. */
template <typename T>
typename TokenTrie<T>::mapped_type&
TokenTrie<T>::operator[](key_type&& key)
{
    return *(add(std::move(key)).first);
}

template <typename T>
typename TokenTrie<T>::size_type TokenTrie<T>::max_size() const noexcept
{
    // Can't return the max_size() of the underlying std::map because
    // there can be more than one in the node tree.
    return std::numeric_limits<difference_type>::max();
}

template <typename T>
void TokenTrie<T>::clear() noexcept
{
    if (root_)
        root_->children.clear();
    size_ = 0;
}

/** @par Exception Safety
    Has no effect if an exception is thrown during insertion of a
    new element. */
template <typename T>
typename TokenTrie<T>::result TokenTrie<T>::insert(const value_type& kv)
{
    return add(kv.first, kv.second);
}

/** @par Exception Safety
    Has no effect if an exception is thrown during insertion of a
    new element. */
template <typename T>
typename TokenTrie<T>::result TokenTrie<T>::insert(value_type&& kv)
{
    return add(std::move(kv.first), std::move(kv.second));
}

/** @par Exception Safety
    Has no effect if an exception is thrown during insertion of the elements.*/
template <typename T>
template <typename I>
void TokenTrie<T>::insert(I first, I last)
{
    return insertRange(IsSpecialTokenTrieIterator<I>{}, first, last);
}

/** @par Exception Safety
    Has no effect if an exception is thrown during insertion of the
    elements. */
template <typename T>
void TokenTrie<T>::insert(std::initializer_list<value_type> list)
{
    insert(list.begin(), list.end());
}

/** @returns A pair with an iterator component to the inserted/updated element,
             and a bool component indicating if insertion took place.
    @par Exception Safety
    Has no effect if an exception is thrown during insertion of a new
    element. */
template <typename T>
template <typename M>
typename TokenTrie<T>::result
TokenTrie<T>::insert_or_assign(const key_type& key, M&& arg)
{
    return put(true, key, std::forward<M>(arg));
}

/** @returns A pair with an iterator component to the inserted/updated element,
             and a bool component indicating if insertion took place.
    @par Exception Safety
    Has no effect if an exception is thrown during insertion of a new
    element. */
template <typename T>
template <typename M>
typename TokenTrie<T>::result
TokenTrie<T>::insert_or_assign(key_type&& key, M&& arg)
{
    return put(true, std::move(key), std::forward<M>(arg));
}

/** @returns A pair with an iterator component to the inserted/existing element,
             and a bool component indicating if insertion took place.
    @par Exception Safety
    Has no effect if an exception is thrown during insertion of a new
    element.
    @note This actually constructs a temporary key-value pair which is then
    split into separate key and value parts. Unlike std::map, it does not
    construct elements in place because only values (and not key-value pairs)
    are stored in the trie's nodes, due to the keys being distrubuted. Use
    TokenTrie::try_emplace instead to construct node values in-place. */
template <typename T>
template <typename... Us>
typename TokenTrie<T>::result TokenTrie<T>::emplace(Us&&... args)
{
    return insert(value_type(std::forward<Us>(args)...));
}

/** @returns A pair with an iterator component to the inserted/existing element,
             and a bool component indicating if insertion took place.
    @par Exception Safety
    Has no effect if an exception is thrown during insertion of a new
    element. */
template <typename T>
template <typename... Us>
typename TokenTrie<T>::result
TokenTrie<T>::try_emplace(const key_type& key, Us&&... args)
{
    return add(key, std::forward<Us>(args)...);
}

/** @returns A pair with an iterator component to the inserted/existing element,
             and a bool component indicating if insertion took place.
    @par Exception Safety
    Has no effect if an exception is thrown during insertion of a new
    element. */
template <typename T>
template <typename... Us>
typename TokenTrie<T>::result
TokenTrie<T>::try_emplace(key_type&& key, Us&&... args)
{
    return add(std::move(key), std::forward<Us>(args)...);
}

/** @returns An iterator following the removed element. */
template <typename T>
typename TokenTrie<T>::iterator TokenTrie<T>::erase(iterator pos)
{
    return eraseAt(pos);
}

/** @returns An iterator following the removed element. */
template <typename T>
typename TokenTrie<T>::iterator TokenTrie<T>::erase(const_iterator pos)
{
    return eraseAt(pos);
}

/** @returns The number of elements erased (0 or 1). */
template <typename T>
typename TokenTrie<T>::size_type TokenTrie<T>::erase(const key_type& key)
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
void TokenTrie<T>::swap(TokenTrie& other) noexcept
{
    root_.swap(other.root_);
    std::swap(size_, other.size_);
    if (root_)
        root_->parent = &sentinel_;
    if (other.root_)
        other.root_->parent = &other.sentinel_;
}

template <typename T>
bool TokenTrie<T>::equals(const TokenTrie& a, const TokenTrie& b) noexcept
{
    if (a.empty() || b.empty())
        return a.empty() == b.empty();

    auto curA = a.rootCursor();
    auto curB = b.rootCursor();
    while (!curA.isSentinel())
    {
        if (curB.isSentinel())
            return false;
        if (curA.iter->first != curB.iter->first)
            return false;
        if (curA.iter->second != curB.iter->second)
            return false;
        curA.advanceToNextNode();
        curB.advanceToNextNode();
    }
    return curB.isSentinel();
}

template <typename T>
bool TokenTrie<T>::differs(const TokenTrie& a, const TokenTrie& b) noexcept
{
    if (a.empty() || b.empty())
        return a.empty() != b.empty();

    auto curA = a.rootCursor();
    auto curB = b.rootCursor();
    while (!curA.isSentinel())
    {
        if (curB.isSentinel())
            return true;
        if (curA.iter->first != curB.iter->first)
            return true;
        if (curA.iter->second != curB.iter->second)
            return true;
        curA.advanceToNextNode();
        curB.advanceToNextNode();
    }
    return !curB.isSentinel();
}

template <typename T>
void TokenTrie<T>::moveFrom(TokenTrie& rhs) noexcept
{
    root_.swap(rhs.root_);
    size_ = rhs.size_;
    rhs.size_ = 0;
    if (root_)
        root_->parent = &sentinel_;
}

template <typename T>
typename TokenTrie<T>::Cursor TokenTrie<T>::rootCursor()
{
    assert(root_ != nullptr);
    return Cursor::begin(*root_);
}

template <typename T>
typename TokenTrie<T>::Cursor TokenTrie<T>::rootCursor() const
{
    assert(root_ != nullptr);
    return Cursor::begin(const_cast<Node&>(*root_));
}

template <typename T>
typename TokenTrie<T>::Cursor TokenTrie<T>::firstTerminalCursor()
{
    if (empty())
        return sentinelCursor();
    auto cursor = rootCursor();
    cursor.advanceToFirstTerminal();
    return cursor;
}

template <typename T>
typename TokenTrie<T>::Cursor TokenTrie<T>::firstTerminalCursor() const
{
    return const_cast<TokenTrie&>(*this).firstTerminalCursor();
}

template <typename T>
typename TokenTrie<T>::Cursor TokenTrie<T>::sentinelCursor()
{
    return Cursor::end(sentinel_);
}

template <typename T>
typename TokenTrie<T>::Cursor TokenTrie<T>::sentinelCursor() const
{
    return Cursor::end(const_cast<Node&>(sentinel_));
}

template <typename T>
typename TokenTrie<T>::Cursor TokenTrie<T>::locate(const key_type& key)
{
    if (empty() || key.empty())
        return sentinelCursor();
    auto cursor = rootCursor();
    cursor.locate(key);
    return cursor;
}

template <typename T>
typename TokenTrie<T>::Cursor
TokenTrie<T>::locate(const key_type& key) const
{
    return const_cast<TokenTrie&>(*this).locate(key);
}

template <typename T>
typename TokenTrie<T>::Cursor
TokenTrie<T>::findLowerBound(const key_type& key) const
{
    if (empty() || key.empty())
        return sentinelCursor();
    auto cursor = rootCursor();
    cursor.findLowerBound(key);
    return cursor;
}

template <typename T>
typename TokenTrie<T>::Cursor
TokenTrie<T>::findUpperBound(const key_type& key) const
{
    if (empty() || key.empty())
        return sentinelCursor();
    auto cursor = rootCursor();
    cursor.findUpperBound(key);
    return cursor;
}

template <typename T>
template <typename I>
std::pair<I, I> TokenTrie<T>::getEqualRange(const key_type& key) const
{
    if (empty() || key.empty())
        return {I{sentinelCursor()}, I{sentinelCursor()}};
    auto range = Cursor::findEqualRange(*root_, key);
    return {I{range.first}, I{range.second}};
}

template <typename T>
template <typename I>
std::pair<I, I> TokenTrie<T>::getMatchRange(const key_type& key) const
{
    if (empty() || key.empty())
        return {I{sentinelCursor()}, I{sentinelCursor()}};

    return {I{rootCursor(), key}, I{sentinelCursor()}};
}

template <typename T>
template <typename I>
void TokenTrie<T>::insertRange(std::true_type, I first, I last)
{
    for (; first != last; ++first)
        add(first.key(), first.value());
}

template <typename T>
template <typename I>
void TokenTrie<T>::insertRange(std::false_type, I first, I last)
{
    for (; first != last; ++first)
        add(first->first, first->second);
}

template <typename T>
template <typename... Us>
typename TokenTrie<T>::result TokenTrie<T>::add(key_type key,
                                                      Us&&... args)
{
    return put(false, std::move(key), std::forward<Us>(args)...);
}

template <typename T>
template <typename... Us>
typename TokenTrie<T>::result
TokenTrie<T>::put(bool clobber, key_type key, Us&&... args)
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
template <typename I>
I TokenTrie<T>::eraseAt(I pos)
{
    auto cursor = pos.cursor_;
    assert(!cursor.isSentinel());
    ++pos;
    cursor.eraseFromHere();
    --size_;
    return pos;
}

template <typename T>
void TokenTrie<T>::scanTree()
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

template <typename T>
template <typename P>
typename TokenTrie<T>::size_type TokenTrie<T>::doEraseIf(P predicate)
{
    using Pair = std::pair<const key_type&, const mapped_type&>;
    auto oldSize = size();
    auto last = end();
    auto iter = begin();
    while (iter != last)
    {
        if (predicate(Pair{iter.key(), iter.value()}))
            iter = erase(iter);
        else
            ++iter;
    }
    return oldSize - size();
}

} // namespace wamp

#endif // CPPWAMP_WILDCARDTRIE_HPP
