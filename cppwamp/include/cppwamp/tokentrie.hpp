/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TOKENTRIE_HPP
#define CPPWAMP_TOKENTRIE_HPP

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
#include <string>
#include <type_traits>
#include <utility>
#include "api.hpp"
#include "tokentrienode.hpp"
#include "internal/tokentrieimpl.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
struct CPPWAMP_HIDDEN TokenTrieIteratorAccess
{
    template <typename L, typename R>
    static bool equals(const L& lhs, const R& rhs)
    {
        return lhs.cursor_ == rhs.cursor_;
    }

    template <typename L, typename R>
    static bool differs(const L& lhs, const R& rhs)
    {
        return lhs.cursor_ != rhs.cursor_;
    }
};

} // namespace internal

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
template <typename N, bool IsMutable>
class CPPWAMP_API TokenTrieMatchIterator
{
public:
    /// The category of the iterator
    using iterator_category = std::forward_iterator_tag;

    /// Type used to identify distance between iterators
    using difference_type = std::ptrdiff_t;

    /// Type of the split token key container associated with this iterator.
    using key_type = typename N::Key;

    /// Type of token associated with this iterator.
    using token_type = typename key_type::value_type;

    /** Type of the mapped value associated with this iterator.
        @note It differs from std::map in that it's not a key-value pair. */
    using value_type = typename N::Value;

    /// Pointer to the mapped value type being iterated over.
    using pointer = typename std::conditional<IsMutable, value_type*,
                                              const value_type*>::type;

    /// Reference to the mapped value type being iterated over.
    using reference = typename std::conditional<IsMutable, value_type&,
                                                const value_type&>::type;

    using Cursor = TokenTrieCursor<N>;

    /** Default constructor. */
    TokenTrieMatchIterator() {}

    /** Implicit conversion from mutable iterator to const iterator. */
    template <bool RM, typename std::enable_if<!IsMutable && RM, int>::type = 0>
    TokenTrieMatchIterator(const TokenTrieMatchIterator<N, RM>& rhs)
        : cursor_(rhs.cursor()) {}

    /** Assignment from mutable iterator to const iterator. */
    template <bool RM, typename std::enable_if<!IsMutable && RM, int>::type = 0>
    TokenTrieMatchIterator&
    operator=(const TokenTrieMatchIterator<N, RM>& rhs)
        {cursor_ = rhs.cursor(); return *this;}

    /** Generates the split token key container associated with the
    current element. */
    key_type key() const {return cursor_.key();}

    /** Obtains the token associated with the current element. */
    token_type token() const {return cursor_.token();}

    /** Accesses the value associated with the current element. */
    reference value() {return cursor_.value();}

    /** Accesses the value associated with the current element. */
    const value_type& value() const {return cursor_.value();}

    /** Obtains a copy of the cursor associated with the current element. */
    Cursor cursor() const {return cursor_;}

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
    using Node = typename Cursor::Node;
    using Access = internal::TokenTrieIteratorAccess;
    using Level = typename key_type::size_type;

    TokenTrieMatchIterator(Cursor endCursor) : cursor_(endCursor) {}

    TokenTrieMatchIterator(Cursor beginCursor, key_type tokens_)
        : key_(std::move(tokens_)), cursor_(beginCursor)
    {
        level_ = cursor_.matchFirst(key_);
    }

    key_type key_;
    Cursor cursor_;
    Level level_ = 0;

    template <typename, typename, typename, typename, typename>
    friend class TokenTrie;

    friend struct internal::TokenTrieIteratorAccess;
};

/** Compares two match iterators for equality.
    @relates TokenTrieMatchIterator */
template <typename N, bool LM, bool RM>
bool operator==(const TokenTrieMatchIterator<N, LM>& lhs,
                const TokenTrieMatchIterator<N, RM>& rhs)
{
    return internal::TokenTrieIteratorAccess::equals(lhs, rhs);
};

/** Compares two match iterators for inequality.
    @relates TokenTrieMatchIterator */
template <typename N, bool LM, bool RM>
bool operator!=(const TokenTrieMatchIterator<N, LM>& lhs,
                const TokenTrieMatchIterator<N, RM>& rhs)
{
    return internal::TokenTrieIteratorAccess::differs(lhs, rhs);
};

template <typename N, bool M>
struct IsSpecialTokenTrieIterator<TokenTrieMatchIterator<N, M>>
    : std::true_type
{};


//------------------------------------------------------------------------------
/** TokenTrie iterator that advances through elements in lexicographic order
    of their respective keys. */
//------------------------------------------------------------------------------
template <typename N, bool IsMutable>
class CPPWAMP_API TokenTrieIterator
{
public:
    /// The category of the iterator.
    using iterator_category = std::forward_iterator_tag;

    /// Type used to identify distance between iterators.
    using difference_type = std::ptrdiff_t;

    /// Type of the split token key container associated with this iterator.
    using key_type = typename N::Key;

    /// Type of token associated with this iterator.
    using token_type = typename key_type::value_type;

    /** Type of the mapped value associated with this iterator.
        @note It differs from std::map in that it's not a key-value pair. */
    using value_type = typename N::Value;

    /// Pointer to the mapped value type being iterated over.
    using pointer = typename std::conditional<IsMutable, value_type*,
                                              const value_type*>::type;

    /// Reference to the mapped value type being iterated over.
    using reference = typename std::conditional<IsMutable, value_type&,
                                                const value_type&>::type;

    using Cursor = TokenTrieCursor<N>;

    /** Default constructor. */
    TokenTrieIterator() {}

    /** Implicit conversion from mutable iterator to const iterator. */
    template <bool M, typename std::enable_if<!IsMutable && M, int>::type = 0>
    TokenTrieIterator(const TokenTrieIterator<N, M>& rhs)
        : cursor_(rhs.cursor()) {}

    /** Implicit conversion from match iterator. */
    template <bool M, typename std::enable_if<(!IsMutable || M), int>::type = 0>
    TokenTrieIterator(const TokenTrieMatchIterator<N, M>& rhs)
        : cursor_(rhs.cursor()) {}

    /** Assignment from mutable iterator to const iterator. */
    template <bool M, typename std::enable_if<!IsMutable && M, int>::type = 0>
    TokenTrieIterator& operator=(const TokenTrieIterator<N, M>& rhs)
        {cursor_ = rhs.cursor_; return *this;}

    /** Assignment from match iterator. */
    template <bool M, typename std::enable_if<(!IsMutable || M), int>::type = 0>
    TokenTrieIterator& operator=(const TokenTrieIterator<N, M>& rhs)
        {cursor_ = rhs.cursor_; return *this;}

    /** Generates the split token key container associated with the
        current element. */
    key_type key() const {return cursor_.key();}

    /** Obtains the token associated with the current element. */
    token_type token() const {return cursor_.token();}

    /** Accesses the value associated with the current element. */
    reference value() {return cursor_.value();}

    /** Accesses the value associated with the current element. */
    const value_type& value() const {return cursor_.value();}

    /** Obtains a copy of the cursor associated with the current element. */
    Cursor cursor() const {return cursor_;}

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
    using Node = typename Cursor::Node;

    TokenTrieIterator(Cursor cursor) : cursor_(cursor) {}

    Cursor cursor_;

    template <typename, typename, typename, typename, typename>
    friend class TokenTrie;

    friend struct internal::TokenTrieIteratorAccess;
};

template <typename N, bool M>
struct IsSpecialTokenTrieIterator<TokenTrieIterator<N, M>>
    : std::true_type
{};

/** Compares two iterators for equality.
    @relates TokenTrieIterator */
template <typename N, bool LM, bool RM>
bool operator==(const TokenTrieIterator<N, LM>& lhs,
                const TokenTrieIterator<N, RM>& rhs)
{
    return internal::TokenTrieIteratorAccess::equals(lhs, rhs);
};

/** Compares two iterators for inequality.
    @relates TokenTrieIterator */
template <typename N, bool LM, bool RM>
bool operator!=(const TokenTrieIterator<N, LM>& lhs,
                const TokenTrieIterator<N, RM>& rhs)
{
    return internal::TokenTrieIteratorAccess::differs(lhs, rhs);
};

/** Compares a match iterator and a regular iterator for equality.
    @relates TokenTrieIterator */
template <typename N, bool LM, bool RM>
bool operator==(const TokenTrieMatchIterator<N, LM>& lhs,
                const TokenTrieIterator<N, RM>& rhs)
{
    return internal::TokenTrieIteratorAccess::equals(lhs, rhs);
};

/** Compares a match iterator and a regular iterator for equality.
    @relates TokenTrieIterator */
template <typename N, bool LM, bool RM>
bool operator==(const TokenTrieIterator<N, LM>& lhs,
                const TokenTrieMatchIterator<N, RM>& rhs)
{
    return internal::TokenTrieIteratorAccess::equals(lhs, rhs);
};

/** Compares a match iterator and a regular iterator for inequality.
    @relates TokenTrieIterator */
template <typename N, bool LM, bool RM>
bool operator!=(const TokenTrieMatchIterator<N, LM>& lhs,
                const TokenTrieIterator<N, RM>& rhs)
{
    return internal::TokenTrieIteratorAccess::differs(lhs, rhs);
};

/** Compares a match iterator and a regular iterator for inequality.
    @relates TokenTrieIterator */
template <typename N, bool LM, bool RM>
bool operator!=(const TokenTrieIterator<N, LM>& lhs,
                const TokenTrieMatchIterator<N, RM>& rhs)
{
    return internal::TokenTrieIteratorAccess::differs(lhs, rhs);
};

//------------------------------------------------------------------------------
template <typename T>
using TokenTrieLocalStorage = internal::TokenTrieValueLocalStorage<T>;

//------------------------------------------------------------------------------
template <typename T>
using TokenTrieHeapStorage = internal::TokenTrieValueHeapStorage<T>;

//------------------------------------------------------------------------------
template <typename T>
struct CPPWAMP_API TokenTrieDefaultPolicy
{
    using value_storage = typename std::conditional<
        (sizeof(T) > sizeof(std::string)),
        TokenTrieHeapStorage<T>,
        TokenTrieLocalStorage<T>>::type;

};

//------------------------------------------------------------------------------
/** Associative container suited for pattern matching, where keys are
    small containers of tokens that have been split from strings
    (e.g. domain names).

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

    Strong exception safety is provided for all modification operations.

    @tparam K Split token container type.
            Must be a Sequence with a `push_back` member.
    @tparam T Mapped value type.
            Must be default constructible. */
//------------------------------------------------------------------------------
template <typename K, typename T, typename = void, typename = void,
          typename P = TokenTrieDefaultPolicy<T>>
class CPPWAMP_API TokenTrie
{
private:
    using Node = TokenTrieNode<K, typename P::value_storage>;
    using NodeAllocatorTraits = std::allocator_traits<typename Node::Allocator>;

    template <typename KV>
    static constexpr bool isInsertable()
    {
        return std::is_constructible<value_type, KV&&>::value;
    }

    template <typename I>
    static constexpr bool isSpecial()
    {
        return IsSpecialTokenTrieIterator<I>::value;
    }

public:
    /** Split token container type used as the key. */
    using key_type = K;

    /** Type of the mapped value. */
    using mapped_type = T;

    using policy_type = P;

    /** Type used to combine a key and its associated value.
        @note Unlike std::map, keys are not stored alongside their associated
              value due to the distributed nature of how keys are stored inside
              the trie. This type is provided to make the interface more closely
              match that of std::map. */
    using value_type = std::pair<const key_type, mapped_type>;

    using cursor_node_type = Node;

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
    using iterator  = TokenTrieIterator<Node, true>;

    /** Immutable iterator type which advances through elements in
        lexicographic order of their respective keys. */
    using const_iterator = TokenTrieIterator<Node, false>;

    /** Mutable iterator type which advances through wildcard matches in
        lexicographic order. */
    using match_iterator = TokenTrieMatchIterator<Node, true>;

    /** Mutable iterator type which advances through wildcard matches in
        lexicographic order. */
    using const_match_iterator = TokenTrieMatchIterator<Node, false>;

    /** Pairs an iterator with the boolean success result of an
        insertion operation. */
    using insert_result = std::pair<iterator, bool>;

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

    using Cursor = TokenTrieCursor<Node>;

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
    TokenTrie() = default;

    /** Copy constructor. */
    TokenTrie(const TokenTrie& rhs) = default;

    /** Move constructor. */
    TokenTrie(TokenTrie&& rhs) noexcept = default;

    /** Constructs using the given iterator range. */
    template <typename I>
    TokenTrie(I first, I last) {insert(first, last);}

    /** Constructs using contents of the given initializer list, where
        each element is a key-value pair. */
    TokenTrie(std::initializer_list<value_type> list)
        {insert(list.begin(), list.end());}

    /** Copy assignment. */
    TokenTrie& operator=(const TokenTrie& rhs) = default;

    /** Move assignment. */
    TokenTrie& operator=(TokenTrie&& rhs) noexcept = default;

    /** Replaces contents with that of the given initializer list,  where
        each element is a key-value pair. */
    TokenTrie& operator=(std::initializer_list<value_type> list)
    {
        TokenTrie temp(list);
        *this = std::move(temp);
        return *this;
    }

    allocator_type get_allocator() const noexcept {return allocator_type();}

    /// @name Element Access
    /// @{

    /** Accesses the element associated with the given key,
        with bounds checking.
        @throws std::out_of_range if the container does not have an element
                with the given key. */
    mapped_type& at(const key_type& key)
        {return checkedAccess(impl_.locate(key));}

    /** Accesses the element associated with the given key,
        with bounds checking.
        @throws std::out_of_range if the container does not have an element
                with the given key. */
    const mapped_type& at(const key_type& key) const
        {return checkedAccess(impl_.locate(key));}

    /** Accesses or inserts an element with the given key. */
    mapped_type& operator[](const key_type& key) {return *(add(key).first);}

    /** Accesses or inserts an element with the given key. */
    mapped_type& operator[](key_type&& key) {return *(add(key).first);}

    /// @name Iterators
    /// @{

    /** Obtains an iterator to the beginning. */
    iterator begin() noexcept {return impl_.firstTerminalCursor();}

    /** Obtains an iterator to the beginning. */
    const_iterator begin() const noexcept {return cbegin();}

    /** Obtains an iterator to the beginning. */
    const_iterator cbegin() const noexcept {return impl_.firstTerminalCursor();}

    /** Obtains an iterator to the end. */
    iterator end() noexcept {return impl_.sentinelCursor();}

    /** Obtains an iterator to the end. */
    const_iterator end() const noexcept {return cend();}

    /** Obtains an iterator to the end. */
    const_iterator cend() const noexcept {return impl_.sentinelCursor();}
    /// @}


    /// @name Capacity
    /// @{

    /** Checks whether the container is empty. */
    bool empty() const noexcept {return impl_.empty();}

    /** Obtains the number of elements. */
    size_type size() const noexcept {return impl_.size();}

    /** Obtains the maximum possible number of elements. */
    size_type max_size() const noexcept
        {return std::numeric_limits<difference_type>::max();}
    /// @}


    /// @name Modifiers
    /// @{

    /** Removes all elements. */
    void clear() noexcept {impl_.clear();}

    /** Inserts an element. */
    insert_result insert(const value_type& kv)
        {return add(kv.first, kv.second);}

    /** Inserts an element. */
    insert_result insert(value_type&& kv)
        {return add(std::move(kv.first), std::move(kv.second));}

    /** Inserts an element.
        Only participates in overload resolution if
        `std::is_constructible_v<value_type, P&&> == true` */
    template <typename KV,
              typename std::enable_if<isInsertable<KV>(), int>::type = 0>
    insert_result insert(KV&& kv)
    {
        value_type pair(std::forward<KV>(kv));
        return add(std::move(pair.first), std::move(pair.second));
    }

    /** Inserts elements from the given iterator range. */
    template <typename I>
    void insert(I first, I last)
        {return insertRange(IsSpecialTokenTrieIterator<I>{}, first, last);}

    /** Inserts elements from the given initializer list of key-value pairs. */
    void insert(std::initializer_list<value_type> list)
        {insert(list.begin(), list.end());}

    /** Inserts an element or assigns to the current element if the key
        already exists. */
    template <typename M>
    insert_result insert_or_assign(const key_type& key, M&& arg)
        {return put(key, std::forward<M>(arg));}

    /** Inserts an element or assigns to the current element if the key
        already exists. */
    template <typename M>
    insert_result insert_or_assign(key_type&& key, M&& arg)
        {return put(std::move(key), std::forward<M>(arg));}

    /** Inserts an element from a key-value pair constructed in-place using
        the given arguments. */
    template <typename... Us>
    insert_result emplace(Us&&... args)
        {return insert(value_type(std::forward<Us>(args)...));}

    /** Inserts in-place only if the key does not exist. */
    template <typename... Us>
    insert_result try_emplace(const key_type& key, Us&&... args)
        {return add(key, std::forward<Us>(args)...);}

    /** Inserts in-place only if the key does not exist. */
    template <typename... Us>
    insert_result try_emplace(key_type&& key, Us&&... args)
        {return add(std::move(key), std::forward<Us>(args)...);}

    /** Erases the element at the given iterator position. */
    iterator erase(iterator pos) {return impl_.erase(pos.cursor());}

    /** Erases the element at the given iterator position. */
    iterator erase(const_iterator pos) {return impl_.erase(pos.cursor());}

    /** Erases the element associated with the given key.
        @returns The number of elements erased (0 or 1). */
    size_type erase(const key_type& key)
    {
        auto cursor = impl_.locate(key);
        bool found = cursor.good();
        if (found)
            impl_.erase(cursor);
        return found ? 1 : 0;
    }

    /** Swaps the contents of this container with the given container. */
    void swap(TokenTrie& other) noexcept {impl_.swap(other.impl_);}
    /// @}

    /// @name Lookup
    /// @{

    /** Returns the number of elements associated with the given key. */
    size_type count(const key_type& key) const {return impl_.locate(key) ? 1 : 0;}

    /** Finds the element associated with the given key. */
    iterator find(const key_type& key) {return impl_.locate(key);}

    /** Finds the element associated with the given key. */
    const_iterator find(const key_type& key) const {return impl_.locate(key);}

    /** Checks if the container contains the element with the given key. */
    bool contains(const key_type& key) const {return bool(impl_.locate(key));}

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
    iterator lower_bound(const key_type& key) {return impl_.lowerBound(key);}

    /** Obtains an iterator to the first element not less than the
        given key. */
    const_iterator lower_bound(const key_type& key) const
        {return impl_.lowerBound(key);}

    /** Obtains an iterator to the first element greater than than the
        given key. */
    iterator upper_bound(const key_type& key) {return impl_.upperBound(key);}

    /** Obtains an iterator to the first element greater than than the
        given key. */
    const_iterator upper_bound(const key_type& key) const
        {return impl_.upperBound(key);}

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
        {return a.impl_.equals(b.impl_);}

    /** Inequality comparison. */
    friend bool operator!=(const TokenTrie& a, const TokenTrie& b) noexcept
        {return a.impl_.differs(b.impl_);}

    /** Non-member swap. */
    friend void swap(TokenTrie& a, TokenTrie& b) noexcept {a.swap(b);}

    /** Erases all elements satisfying given criteria. */
    template <typename F>
    friend size_type erase_if(TokenTrie& t, F predicate)
        {return t.doEraseIf(std::move(predicate));}

private:
    template <typename C>
    static auto checkedAccess(C&& cursor)
        -> decltype(std::forward<C>(cursor).value())
    {
        if (!cursor)
            throw std::out_of_range("wamp::TokenTrie::at key out of range");
        return std::forward<C>(cursor).value();
    }

    template <typename... Us>
    insert_result add(key_type key, Us&&... args)
    {
        auto r = impl_.put(false, std::move(key), std::forward<Us>(args)...);
        return {iterator{r.first}, r.second};
    }

    template <typename... Us>
    insert_result put(key_type key, Us&&... args)
    {
        auto r = impl_.put(true, std::move(key), std::forward<Us>(args)...);
        return {iterator{r.first}, r.second};
    }

    template <typename I>
    std::pair<I, I> getEqualRange(const key_type& key) const
    {
        auto er = impl_.equalRange(key);
        return {I{er.first}, I{er.second}};
    }

    template <typename I>
    std::pair<I, I> getMatchRange(const key_type& key) const
    {
        if (empty() || key.empty())
            return {I{impl_.sentinelCursor()}, I{impl_.sentinelCursor()}};

        return {I{impl_.rootCursor(), key}, I{impl_.sentinelCursor()}};
    }

    template <typename I>
    void insertRange(std::true_type, I first, I last)
    {
        for (; first != last; ++first)
            add(first.key(), first.value());
    }

    template <typename I>
    void insertRange(std::false_type, I first, I last)
    {
        for (; first != last; ++first)
            add(first->first, first->second);
    }

    template <typename F>
    size_type doEraseIf(F predicate)
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

    internal::TokenTrieImpl<K, T, P> impl_;
};

} // namespace wamp

#endif // CPPWAMP_TOKENTRIE_HPP
