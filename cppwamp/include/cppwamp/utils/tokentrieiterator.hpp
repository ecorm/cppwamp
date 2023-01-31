/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_UTILS_TOKENTRIEITERATOR_HPP
#define CPPWAMP_UTILS_TOKENTRIEITERATOR_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains TokenTrie iterator facilities. */
//------------------------------------------------------------------------------

#include <cstddef>
#include <functional>
#include <iterator>
#include <tuple>
#include <type_traits>
#include <utility>
#include "tokentrienode.hpp"

namespace wamp
{

namespace utils
{

namespace internal
{

template <typename I, typename Enable = void>
struct IsTokenTrieIterator : std::false_type {};

}

//------------------------------------------------------------------------------
/** Proxy class representing a reference to a TokenTrie key-value pair.
    Mimics a `std::pair<const K, V>` reference. */
//------------------------------------------------------------------------------
template <typename K, typename V, bool IsMutable>
struct TokenTrieKeyValueProxy
{
    /// Key type.
    using first_type = K;

    /// Reference wrapper type to the mapped value.
    using second_type =
        std::reference_wrapper<typename std::conditional<IsMutable,
                                                         V, const V>::type>;

    /// Contains the element's key.
    first_type first;

    /// Contains a reference to the mapped value.
    second_type second;

    /// Default constructor.
    TokenTrieKeyValueProxy() = default;

    /// Constructor taking the key and mapped value.
    template <typename U>
    explicit TokenTrieKeyValueProxy(K k, U&& v)
        : first(std::move(k)), second(std::forward<U>(v))
    {}

    /// Implicit conversion to the equivalent non-proxy pair type.
    operator std::pair<const K, V>() const
        {return std::pair<const K, V>(first, second.get());}

    /// Swap
    void swap(TokenTrieKeyValueProxy& other)
    {
        TokenTrieKeyValueProxy temp(other);
        other = std::move(*this);
        *this = std::move(temp);
    }
};

/// Non-member swap
template <typename K, typename V, bool M>
void swap(TokenTrieKeyValueProxy<K,V,M>& a, TokenTrieKeyValueProxy<K,V,M>&b)
{
    a.swap(b);
}

/** @name Comparison Operators.
    @{ */

/** @relates TokenTrieKeyValueProxy */
template <typename K, typename V, bool M>
bool operator==(const TokenTrieKeyValueProxy<K,V,M>& lhs,
                const std::pair<const K, V>& rhs)
{
    return std::tie(lhs.first, lhs.second.get()) ==
           std::tie(rhs.first, rhs.second);
}

/** @relates TokenTrieKeyValueProxy */
template <typename K, typename V, bool M>
bool operator==(const std::pair<const K, V>& lhs,
                const TokenTrieKeyValueProxy<K,V,M>& rhs)
{
    return std::tie(lhs.first, lhs.second) ==
           std::tie(rhs.first, rhs.second.get());}

/** @relates TokenTrieKeyValueProxy */
template <typename K, typename V, bool M>
bool operator!=(const TokenTrieKeyValueProxy<K,V,M>& lhs,
                const std::pair<const K, V>& rhs)
{
    return std::tie(lhs.first, lhs.second.get()) !=
           std::tie(rhs.first, rhs.second);
}

/** @relates TokenTrieKeyValueProxy */
template <typename K, typename V, bool M>
bool operator!=(const std::pair<const K, V>& lhs,
                const TokenTrieKeyValueProxy<K,V,M>& rhs)
{
    return std::tie(lhs.first, lhs.second) !=
           std::tie(rhs.first, rhs.second.get());
}

/** @relates TokenTrieKeyValueProxy */
template <typename K, typename V, bool M>
bool operator<(const TokenTrieKeyValueProxy<K,V,M>& lhs,
               const std::pair<const K, V>& rhs)
{
    return std::tie(lhs.first, lhs.second.get()) <
           std::tie(rhs.first, rhs.second);
}

/** @relates TokenTrieKeyValueProxy */
template <typename K, typename V, bool M>
bool operator<(const std::pair<const K, V>& lhs,
               const TokenTrieKeyValueProxy<K,V,M>& rhs)
{
    return std::tie(lhs.first, lhs.second) <
           std::tie(rhs.first, rhs.second.get());
}

/** @relates TokenTrieKeyValueProxy */
template <typename K, typename V, bool M>
bool operator<=(const TokenTrieKeyValueProxy<K,V,M>& lhs,
                const std::pair<const K, V>& rhs)
{
    return std::tie(lhs.first, lhs.second.get()) <=
           std::tie(rhs.first, rhs.second);
}

/** @relates TokenTrieKeyValueProxy */
template <typename K, typename V, bool M>
bool operator<=(const std::pair<const K, V>& lhs,
                const TokenTrieKeyValueProxy<K,V,M>& rhs)
{
    return std::tie(lhs.first, lhs.second) <=
           std::tie(rhs.first, rhs.second.get());
}

/** @relates TokenTrieKeyValueProxy */
template <typename K, typename V, bool M>
bool operator>(const TokenTrieKeyValueProxy<K,V,M>& lhs,
               const std::pair<const K, V>& rhs)
{
    return std::tie(lhs.first, lhs.second.get()) >
           std::tie(rhs.first, rhs.second);
}

/** @relates TokenTrieKeyValueProxy */
template <typename K, typename V, bool M>
bool operator>(const std::pair<const K, V>& lhs,
               const TokenTrieKeyValueProxy<K,V,M>& rhs)
{
    return std::tie(lhs.first, lhs.second) >
           std::tie(rhs.first, rhs.second.get());
}

/** @relates TokenTrieKeyValueProxy */
template <typename K, typename V, bool M>
bool operator>=(const TokenTrieKeyValueProxy<K,V,M>& lhs,
                const std::pair<const K, V>& rhs)
{
    return std::tie(lhs.first, lhs.second.get()) >=
           std::tie(rhs.first, rhs.second);
}

/** @relates TokenTrieKeyValueProxy */
template <typename K, typename V, bool M>
bool operator>=(const std::pair<const K, V>& lhs,
                const TokenTrieKeyValueProxy<K,V,M>& rhs)
{
    return std::tie(lhs.first, lhs.second) >=
           std::tie(rhs.first, rhs.second.get());
}
/// @}

//------------------------------------------------------------------------------
/** Proxy class representing a pointer to a TokenTrie key-value pair. */
//------------------------------------------------------------------------------
template <typename K, typename V, bool IsMutable>
class TokenTrieKeyValuePointer
{
public:
    using proxy = TokenTrieKeyValueProxy<K, V, IsMutable>;

    /** Accesses the key (first) or value (second) of an element. */
    proxy* operator->() {return &proxy_;}

private:
    template <typename T>
    TokenTrieKeyValuePointer(K key, T&& value)
        : proxy_(std::move(key), std::forward<T>(value))
    {}

    proxy proxy_;

    template <typename, bool> friend class TokenTrieIterator;
};

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
    using key_type = typename N::key_type;

    /// Type of token associated with this iterator.
    using token_type = typename N::token_type;

    /// Type of element associated with this iterator.
    using mapped_type = typename N::mapped_type;

    /// Reference type to the value being iterated over.
    using mapped_reference =
        typename std::conditional<IsMutable, mapped_type&,
                                  const mapped_type&>::type;

    /** Key-value pair type. */
    using value_type = std::pair<const key_type, mapped_type>;

    /// Pointer type to the key-value pair being iterated over.
    using pointer = TokenTrieKeyValuePointer<key_type, mapped_type, IsMutable>;

    /// Pointer type to the immutable key-value pair being iterated over.
    using const_pointer = TokenTrieKeyValuePointer<key_type, mapped_type, false>;

    /// Reference type to the key-value pair being iterated over.
    using reference = TokenTrieKeyValueProxy<key_type, mapped_type, IsMutable>;

    /// Reference type to the immutable key-value pair being iterated over.
    using const_reference = TokenTrieKeyValueProxy<key_type, mapped_type,
                                                   false>;

    /// Type if the underlying cursor used to traverse nodes.
    using cursor_type = TokenTrieCursor<N, IsMutable>;

    /** Default constructor. */
    TokenTrieIterator() {}

    /** Conversion from mutable iterator to const iterator. */
    template <bool M, typename std::enable_if<!IsMutable && M, int>::type = 0>
    TokenTrieIterator(const TokenTrieIterator<N, M>& rhs)
        : cursor_(rhs.cursor()) {}

    /** Assignment from mutable iterator to const iterator. */
    template <bool M, typename std::enable_if<!IsMutable && M, int>::type = 0>
    TokenTrieIterator& operator=(const TokenTrieIterator<N, M>& rhs)
        {cursor_ = rhs.cursor_; return *this;}

    /** Generates the split token key container associated with the
        current element. */
    key_type key() const {return cursor_.key();}

    /** Obtains the token associated with the current element. */
    token_type token() const {return cursor_.token();}

    /** Accesses the key-value pair associated with the current element. */
    mapped_reference value() {return cursor_.value();}

    /** Accesses the key-value pair associated with the current element. */
    const mapped_type& value() const {return cursor_.value();}

    /** Obtains a copy of the cursor associated with the current element. */
    cursor_type cursor() const {return cursor_;}

    /** Accesses the key-value pair associated with the current element.
        @note The key is generated from the tokens along the node's path. */
    reference operator*() {return reference{key(), value()};}

    /** Accesses the key-value pair associated with the current element.
        @note The key is generated from the tokens along the node's path. */
    const_reference operator*() const {return const_reference{key(), value()};}

    /** Accesses a member of the value associated with the current element. */
    pointer operator->() {return pointer{key(), value()};}

    /** Accesses a member of the value associated with the current element. */
    const_pointer operator->() const {return const_pointer{key(), value()};}

    /** Prefix increment, advances to the next key in lexigraphic order. */
    TokenTrieIterator& operator++()
        {cursor_.advance_depth_first_to_next_element(); return *this;}

    /** Postfix increment, advances to the next key in lexigraphic order. */
    TokenTrieIterator operator++(int)
        {auto temp = *this; ++(*this); return temp;}

private:
    using Node = typename cursor_type::node_type;

    TokenTrieIterator(cursor_type cursor) : cursor_(cursor) {}

    cursor_type cursor_;

    template <typename, typename, typename, typename>
    friend class TokenTrie;
};

namespace internal
{

template <typename N, bool M>
struct IsTokenTrieIterator<TokenTrieIterator<N, M>> : std::true_type {};

}

/** Compares two iterators for equality.
    @relates TokenTrieIterator */
template <typename N, bool LM, bool RM>
bool operator==(const TokenTrieIterator<N, LM>& lhs,
                const TokenTrieIterator<N, RM>& rhs)
{
    return lhs.cursor() == rhs.cursor();
};

/** Compares two iterators for inequality.
    @relates TokenTrieIterator */
template <typename N, bool LM, bool RM>
bool operator!=(const TokenTrieIterator<N, LM>& lhs,
                const TokenTrieIterator<N, RM>& rhs)
{
    return lhs.cursor() != rhs.cursor();
};

} // namespace utils

} // namespace wamp

#endif // CPPWAMP_UTILS_TOKENTRIEITERATOR_HPP
