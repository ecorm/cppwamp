/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_UTILS_TOKENTRIE_HPP
#define CPPWAMP_UTILS_TOKENTRIE_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains the TokenTrie template class. */
//------------------------------------------------------------------------------

#include <cstddef>
#include <initializer_list>
#include <limits>
#include <memory>
#include <utility>
#include "tokentrieiterator.hpp"
#include "tokentrienode.hpp"
#include "../traits.hpp"
#include "../internal/tokentrieimpl.hpp"

namespace wamp
{

namespace utils
{

//------------------------------------------------------------------------------
struct TokenTrieDefaultOrdering
{
    using is_transparent = std::true_type;

    template <typename L, typename R>
    bool operator()(const L& lhs, const R& rhs) const {return lhs < rhs;}
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

    This trie implementation does not implement compaction (like in a radix
    tree) in order to avoid invalidating iterators upon modification.

    This container supports `std::scoped_allocator_adapter` so that the key
    and mapped types will respectively use the user-provided allocator if they
    specialize `std::uses_allocator`.

    @tparam K Split token container type.
            Must be a Sequence with a `push_back` member function.
    @tparam T Mapped value type. Must be default-constructible.
    @tparam C Token compare function.
    @tparam A Allocator */
//------------------------------------------------------------------------------
template <typename K,
          typename T,
          typename C = TokenTrieDefaultOrdering,
          typename A = std::allocator<T>>
class TokenTrie
{
private:
    using Impl = internal::TokenTrieImpl<K, T, C, A>;
    using Node = typename Impl::Node;

    template <typename KV>
    static constexpr bool isInsertable()
    {
        return std::is_constructible<value_type, KV&&>::value;
    }

public:
    /// Split token container type used as the key.
    using key_type = K;

    /// Type of the mapped value.
    using mapped_type = T;

    /** Type used to combine a key and its associated value.
        @note Unlike std::map, keys are not stored alongside their associated
              value due to the distributed nature of how keys are stored inside
              the trie. This type is provided to make the interface more closely
              match that of std::map. */
    using value_type = std::pair<const key_type, mapped_type>;

    /// Type used to count the number of elements in the container.
    using size_type = typename Node::tree_type::size_type;

    /// Type used to identify distance between iterators.
    using difference_type = std::ptrdiff_t;

    /// Comparison function that determines how keys are sorted.
    using key_compare = C;

    /// Allocator type.
    using allocator_type = A;

    /// Reference to a key-value pair.
    using reference = TokenTrieKeyValueProxy<key_type, mapped_type, true>;

    /// Reference to an immutable key-value pair.
    using const_reference = TokenTrieKeyValueProxy<key_type, mapped_type, false>;

    /// Pointer to key-value pair
    using pointer = TokenTrieKeyValuePointer<key_type, mapped_type, true>;

    /// Pointer to an immutable key-value pair
    using const_pointer = TokenTrieKeyValuePointer<key_type, mapped_type, false>;

    /** Mutable iterator type which advances through elements in lexicographic
        order of their respective keys. */
    using iterator = TokenTrieIterator<Node, true>;

    /** Immutable iterator type which advances through elements in
        lexicographic order of their respective keys. */
    using const_iterator = TokenTrieIterator<Node, false>;

    /** Pairs an iterator with the boolean success result of an
        insertion operation. */
    using insert_result = std::pair<iterator, bool>;

    /** Pair of mutable iterators corresponding to a range. */
    using range_type = std::pair<iterator, iterator>;

    /** Pair of immutable iterators corresponding to a range. */
    using const_range_type = std::pair<const_iterator, const_iterator>;

    /** Mutable cursor type used for traversing nodes. */
    using cursor = TokenTrieCursor<Node, true>;

    /** Immutable cursor type used for traversing nodes. */
    using const_cursor = TokenTrieCursor<Node, false>;

    /** Function object type used for sorting key-value pairs
        in lexicographic order of their keys. */
    using value_compare = typename Impl::ValueComp;

    /** Default constructor. */
    TokenTrie() : TokenTrie(key_compare{}, allocator_type{}) {}

    /** Constructor taking a compare function and allocator. */
    explicit TokenTrie(const key_compare& c, const allocator_type& a = {} )
        : impl_(c, a) {}

    /** Constructor taking an allocator. */
    explicit TokenTrie(const allocator_type& alloc)
        : TokenTrie(key_compare{}, alloc) {}

    /** Copy constructor. */
    TokenTrie(const TokenTrie& rhs) : impl_(rhs.impl_) {}

    /** Copy constructor taking an allocator. */
    TokenTrie(const TokenTrie& rhs, const allocator_type& a)
        : impl_(rhs, a) {}

    /** Move constructor. */
    TokenTrie(TokenTrie&& rhs)
        noexcept(std::is_nothrow_move_constructible<value_compare>::value)
        : impl_(std::move(rhs.impl_)) {}

    /** Move constructor taking an allocator. */
    TokenTrie(TokenTrie&& rhs, const allocator_type& a) noexcept
        : impl_(std::move(rhs), a) {}

    /** Constructs using the given iterator range. */
    template <typename I>
    TokenTrie(I first, I last, const key_compare& c = {},
              const allocator_type& a = {})
        : TokenTrie(c, a) {insert(first, last);}

    /** Constructs using the given iterator range. */
    template <typename I>
    TokenTrie(I first, I last, const allocator_type& a)
        : TokenTrie(a) {insert(first, last);}

    /** Constructs using contents of the given initializer list, where
        each element is a key-value pair. */
    TokenTrie(std::initializer_list<value_type> list, const key_compare& c = {},
              const allocator_type& a = {})
        : TokenTrie(c, a) {insert(list.begin(), list.end());}

    /** Constructs using contents of the given initializer list, where
        each element is a key-value pair. */
    TokenTrie(std::initializer_list<value_type> list, const allocator_type& a)
        : TokenTrie(a) {insert(list.begin(), list.end());}

    /** Copy assignment. */
    TokenTrie& operator=(const TokenTrie& rhs) = default;

    /** Move assignment. */
    TokenTrie& operator=(TokenTrie&& rhs)
        noexcept(std::allocator_traits<A>::is_always_equal::value &&
                     std::is_nothrow_move_assignable<value_compare>::value)
        = default;

    /** Replaces contents with that of the given initializer list,  where
        each element is a key-value pair. */
    TokenTrie& operator=(std::initializer_list<value_type> list)
    {
        TokenTrie temp(list);
        *this = std::move(temp);
        return *this;
    }

    /** Obtains the container's allocator. */
    allocator_type get_allocator() const noexcept {return impl_.allocator();}

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
    mapped_type& operator[](const key_type& key)
        {return add(key).first.value();}

    /** Accesses or inserts an element with the given key. */
    mapped_type& operator[](key_type&& key) {return add(key).first.value();}

    /// @name Iterators
    /// @{

    /** Obtains an iterator to the beginning. */
    iterator begin() noexcept {return impl_.firstValueCursor();}

    /** Obtains an iterator to the beginning. */
    const_iterator begin() const noexcept {return cbegin();}

    /** Obtains an iterator to the beginning. */
    const_iterator cbegin() const noexcept {return impl_.firstValueCursor();}

    /** Obtains an iterator to the end. */
    iterator end() noexcept {return impl_.sentinelCursor();}

    /** Obtains an iterator to the end. */
    const_iterator end() const noexcept {return cend();}

    /** Obtains an iterator to the end. */
    const_iterator cend() const noexcept {return impl_.sentinelCursor();}
    /// @}

    /// @name Cursors
    /// @{

    /** Obtains a cursor to the root node, or the sentinel node if empty. */
    cursor root() noexcept {return impl_.rootCursor();}

    /** Obtains a cursor to the root node, or the sentinel node if empty. */
    const_cursor root() const noexcept {return croot();}

    /** Obtains a cursor to the root node, or the sentinel node if empty. */
    const_cursor croot() const noexcept {return impl_.rootCursor();}

    /** Obtains a cursor to the first value node, or the sentinel node
        if empty. */
    cursor first() noexcept {return impl_.firstValueCursor();}

    /** Obtains a cursor to the first value node, or the sentinel node
        if empty. */
    const_cursor first() const noexcept {return cfirst();}

    /** Obtains a cursor to the first value node, or the sentinel node
        if empty. */
    const_cursor cfirst() const noexcept {return impl_.firstValueCursor();}

    /** Obtains a cursor to the sentinel node. */
    cursor sentinel() noexcept {return impl_.sentinelCursor();}

    /** Obtains a cursor to the sentinel node. */
    const_cursor sentinel() const noexcept {return csentinel();}

    /** Obtains a cursor to the sentinel node. */
    const_cursor csentinel() const noexcept {return impl_.sentinelCursor();}
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
        {return insertRange(internal::IsTokenTrieIterator<I>{}, first, last);}

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
    void swap(TokenTrie& other)
        noexcept(std::allocator_traits<A>::is_always_equal::value &&
                 isNothrowSwappable<C>())
        {impl_.swap(other.impl_);}
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
        {return getEqualRange<iterator>(*this, key);}

    /** Obtains the range of elements lexicographically matching
        the given key.*/
    const_range_type equal_range(const key_type& key) const
        {return getEqualRange<const_iterator>(*this, key);}

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

    /** Obtains the function that compares keys. */
    key_compare key_comp() const {return impl_.keyComp();}

    /** Obtains the function that compares keys in value_type objects. */
    value_compare value_comp() const {return impl_.valueComp();}

    /// @}

    /** Equality comparison. */
    friend bool operator==(const TokenTrie& a, const TokenTrie& b)
        {return a.impl_.equals(b.impl_);}

    /** Inequality comparison. */
    friend bool operator!=(const TokenTrie& a, const TokenTrie& b)
        {return a.impl_.differs(b.impl_);}

    /** Non-member swap. */
    friend void swap(TokenTrie& a, TokenTrie& b) noexcept {a.swap(b);}

    /** Erases all elements satisfying given criteria. */
    template <typename F>
    friend size_type erase_if(TokenTrie& t, F predicate)
        {return t.doEraseIf(std::move(predicate));}

private:
    template <typename TCursor>
    static auto checkedAccess(TCursor&& cursor)
        -> decltype(std::forward<TCursor>(cursor).value())
    {
        if (!cursor)
            throw std::out_of_range("wamp::TokenTrie::at key out of range");
        return std::forward<TCursor>(cursor).value();
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

    template <typename I, typename TSelf>
    std::pair<I, I> getEqualRange(TSelf& self, const key_type& key) const
    {
        auto er = self.impl_.equalRange(key);
        return {I{er.first}, I{er.second}};
    }

    template <typename I, typename TSelf>
    std::pair<I, I> getMatchRange(TSelf& self, const key_type& key) const
    {
        if (self.empty() || key.empty())
            return {I{self.impl_.sentinelCursor()},
                    I{self.impl_.sentinelCursor()}};

        return {I{self.impl_.rootCursor(), key},
                I{self.impl_.sentinelCursor()}};
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

    internal::TokenTrieImpl<K, T, C, A> impl_;
};

} // namespace utils

} // namespace wamp


namespace std
{

template <typename K, typename T, typename C, typename A, typename Alloc>
struct uses_allocator<wamp::utils::TokenTrie<K,T,C,A>, Alloc> :
    std::is_convertible<Alloc, A>
{};

} // namespace std

#endif // CPPWAMP_UTILS_TOKENTRIE_HPP
