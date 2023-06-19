/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_UTILS_TOKENTRIEMAPITERATOR_HPP
#define CPPWAMP_UTILS_TOKENTRIEMAPITERATOR_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains TokenTrieMap iterator facilities. */
//------------------------------------------------------------------------------

#include <cstddef>
#include <functional>
#include <iterator>
#include <tuple>
#include <utility>
#include "tokentriemapnode.hpp"
#include "../traits.hpp"

namespace wamp
{

namespace utils
{

//------------------------------------------------------------------------------
/** TokenTrieMap iterator that advances through elements in lexicographic order
    of their respective keys. */
//------------------------------------------------------------------------------
template <typename N, bool IsMutable>
class TokenTrieMapIterator
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
    using mapped_reference = Conditional<IsMutable, mapped_type&,
                                         const mapped_type&>;

    /** Key-value pair type. */
    using value_type = std::pair<const key_type, mapped_type>;

    /// Pointer type to the key-value pair being iterated over.
    using pointer = Conditional<IsMutable, typename N::element_pointer,
                                typename N::const_element_pointer>;

    /// Pointer type to the immutable key-value pair being iterated over.
    using const_pointer = typename N::const_element_pointer;

    /// Reference type to the key-value pair being iterated over.
    using reference = Conditional<IsMutable, value_type&, const value_type&>;

    /// Reference type to the immutable key-value pair being iterated over.
    using const_reference = const value_type&;

    /// Type if the underlying cursor used to traverse nodes.
    using cursor_type = TokenTrieMapCursor<N, IsMutable>;

    /** Default constructor. */
    TokenTrieMapIterator() = default;

    /** Conversion from mutable iterator to const iterator. */
    template <bool M, CPPWAMP_NEEDS(!IsMutable && M) = 0>
    TokenTrieMapIterator(const TokenTrieMapIterator<N, M>& rhs)
        : cursor_(rhs.cursor()) {}

    /** Assignment from mutable iterator to const iterator. */
    template <bool M, CPPWAMP_NEEDS(!IsMutable && M) = 0>
    TokenTrieMapIterator& operator=(const TokenTrieMapIterator<N, M>& rhs)
        {cursor_ = rhs.cursor_; return *this;}

    /** Obtains a copy of the cursor associated with the current element. */
    cursor_type cursor() const {return cursor_;}

    /** Accesses the key-value pair associated with the current element.
        @note The key is generated from the tokens along the node's path. */
    reference operator*() {return cursor_.element();}

    /** Accesses the key-value pair associated with the current element.
        @note The key is generated from the tokens along the node's path. */
    const_reference operator*() const {return cursor_.element();}

    /** Accesses a member of the key-value pair associated with the
        current element. */
    pointer operator->()
    {
        assert(cursor_.has_element());
        return cursor_.target_->second.element_;
    }

    /** Accesses a member of the key-value pair associated with the
        current element. */
    const_pointer operator->() const
    {
        assert(cursor_.has_element());
        return cursor_.target_->second.element_;
    }

    /** Prefix increment, advances to the next key in lexigraphic order. */
    TokenTrieMapIterator& operator++()
        {cursor_.advance_depth_first_to_next_element(); return *this;}

    /** Postfix increment, advances to the next key in lexigraphic order. */
    TokenTrieMapIterator operator++(int)
        {auto temp = *this; ++(*this); return temp;}

private:
    using Node = typename cursor_type::node_type;

    TokenTrieMapIterator(cursor_type cursor) : cursor_(cursor) {}

    cursor_type cursor_;

    template <typename, typename, typename, typename>
    friend class TokenTrieMap;
};

/** Compares two iterators for equality.
    @relates TokenTrieMapIterator */
template <typename N, bool LM, bool RM>
bool operator==(const TokenTrieMapIterator<N, LM>& lhs,
                const TokenTrieMapIterator<N, RM>& rhs)
{
    return lhs.cursor() == rhs.cursor();
};

/** Compares two iterators for inequality.
    @relates TokenTrieMapIterator */
template <typename N, bool LM, bool RM>
bool operator!=(const TokenTrieMapIterator<N, LM>& lhs,
                const TokenTrieMapIterator<N, RM>& rhs)
{
    return lhs.cursor() != rhs.cursor();
};

} // namespace utils

} // namespace wamp

#endif // CPPWAMP_UTILS_TOKENTRIEMAPITERATOR_HPP
