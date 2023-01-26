/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TOKENTRIENODE_HPP
#define CPPWAMP_TOKENTRIENODE_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains the TokenTrie node and cursor facilities. */
//------------------------------------------------------------------------------

#include <algorithm>
#include <cassert>
#include <initializer_list>
#include <map>
#include <type_traits>
#include <utility>

#include "api.hpp"
#include "error.hpp"
#include "tagtypes.hpp"
#include "internal/tokentrievaluestorage.hpp"

namespace wamp
{

namespace internal
{
    template <typename, typename, typename, typename> class TokenTrieImpl;
}

//------------------------------------------------------------------------------
template <typename S>
class CPPWAMP_API TokenTrieOptionalValue
{
private:
    using Traits = internal::TokenTrieValueTraits<typename S::value_type,
                                                  TokenTrieOptionalValue>;

public:
    using value_type = typename S::value_type;

    TokenTrieOptionalValue() noexcept = default;

    TokenTrieOptionalValue(const TokenTrieOptionalValue&) = default;

    TokenTrieOptionalValue(TokenTrieOptionalValue&&) = default;

    template <typename U,
             typename std::enable_if<
                 Traits::template isConvertible<U>(), int>::type = 0>
    TokenTrieOptionalValue(U&& x) : value_(in_place, std::forward<U>(x)) {}

    template <typename U,
             typename std::enable_if<
                 Traits::template isConstructible<U>(), int>::type = 0>
    explicit TokenTrieOptionalValue(U&& x)
        : value_(in_place, std::forward<U>(x))
    {}

    template <typename... Us>
    explicit TokenTrieOptionalValue(in_place_t, Us&&... args)
        : value_(in_place_t{}, std::forward<Us>(args)...)
    {}

    template <typename E, typename... Us>
    explicit TokenTrieOptionalValue(in_place_t, std::initializer_list<E> list,
                                    Us&&... args)
        : value_(in_place_t{}, list, std::forward<Us>(args)...)
    {}

    TokenTrieOptionalValue& operator=(const TokenTrieOptionalValue&) = default;

    TokenTrieOptionalValue& operator=(TokenTrieOptionalValue&&) = default;

    template <typename U,
             typename std::enable_if<
                 Traits::template isAssignable<U>(), int>::type = 0>
    TokenTrieOptionalValue& operator=(U&& x)
    {
        value_.assign(std::forward<U>(x));
        return *this;
    }

    bool has_value() const noexcept {return value_.has_value();}

    explicit operator bool() const noexcept {return has_value();}

    value_type& operator*() {return get();}

    const value_type& operator*() const {return get();}

    value_type& value() & {return checkedValue(*this);}

    value_type&& value() && {return checkedValue(*this);}

    const value_type& value() const & {return checkedValue(*this);}

    const value_type&& value() const && {return checkedValue(*this);}

    template <typename U>
    value_type value_or(U&& fallback) const &
    {
        if (has_value())
            return get();
        else
            return static_cast<value_type>(std::forward<U>(fallback));
    }

    template <typename U>
    value_type value_or(U&& fallback) &&
    {
        if (has_value())
            return std::move(get());
        else
            return static_cast<value_type>(std::forward<U>(fallback));
    }

    template <typename... Us>
    value_type& emplace(Us&&... args)
    {
        value_.emplace(std::forward<Us>(args)...);
        return value_.get();
    }

    template <typename E, typename... Us>
    value_type& emplace(std::initializer_list<E> list, Us&&... args)
    {
        value_.emplace(list, std::forward<Us>(args)...);
        return value_.get();
    }

    void reset() {value_.reset();}

    void swap(TokenTrieOptionalValue& rhs)
    {
        if (has_value())
        {
            if (rhs.has_value())
            {
                using std::swap;
                swap(value_.get(), rhs.value_.get());
            }
            else
            {
                rhs.value_.assign(std::move(value_.get()));
                reset();
            }
        }
        else if (rhs.has_value())
        {
            value_.assign(std::move(rhs.value_.get()));
            rhs.reset();
        }
    }

    friend void swap(TokenTrieOptionalValue& lhs, TokenTrieOptionalValue& rhs)
    {
        lhs.swap(rhs);
    }

    friend bool operator==(const TokenTrieOptionalValue& lhs,
                           const TokenTrieOptionalValue& rhs)
    {
        if (!lhs.has_value())
            return !rhs.has_value();
        return rhs.has_value() && (lhs.get() == rhs.get());
    }

    template <typename U>
    friend bool operator==(const TokenTrieOptionalValue& lhs,
                           const value_type& rhs)
    {
        return lhs.has_value() && (lhs.get() == rhs);
    }

    template <typename U>
    friend bool operator==(const value_type& lhs,
                           const TokenTrieOptionalValue& rhs)
    {
        return rhs.has_value() && (rhs.get() == lhs);
    }

    friend bool operator!=(const TokenTrieOptionalValue& lhs,
                           const TokenTrieOptionalValue& rhs)
    {
        if (!lhs.has_value())
            return rhs.has_value();
        return !rhs.has_value() || (lhs.get() != rhs.get());
    }

    template <typename U>
    friend bool operator!=(const TokenTrieOptionalValue& lhs,
                           const value_type& rhs)
    {
        return !lhs.has_value() || (lhs.get() != rhs);
    }

    template <typename U>
    friend bool operator!=(const value_type& lhs,
                           const TokenTrieOptionalValue& rhs)
    {
        return !rhs.has_value() || (rhs.get() != lhs);
    }

private:
    using Storage = S;

    value_type& get()
    {
        assert(has_value());
        return value_.get();
    }

    const value_type& get() const
    {
        assert(has_value());
        return value_.get();
    }

    template <typename Self>
    static auto checkedValue(Self&& s)
        -> decltype(std::forward<Self>(s).get())
    {
        CPPWAMP_LOGIC_CHECK(s.has_value(), "TokenTrieOptionalValue bad access");
        return std::forward<Self>(s).get();
    }

    Storage value_;
};

//------------------------------------------------------------------------------
template <typename K, typename S, typename C>
class CPPWAMP_API TokenTrieNode
{
public:
    using key_type = K;
    using value_storage = S;
    using key_compare = C;
    using optional_value = TokenTrieOptionalValue<value_storage>;
    using value_type = typename optional_value::value_type;
    using token_type = typename key_type::value_type;
    using tree_type = std::map<token_type, TokenTrieNode, key_compare>;
    using allocator_type = typename tree_type::allocator_type;

    TokenTrieNode() : position_(children_.end()) {}

    template <typename... Us>
    TokenTrieNode(in_place_t, Us&&... args)
        : element_(in_place, std::forward<Us>(args)...)
    {}

    bool is_sentinel() const noexcept {return parent_ == nullptr;}

    bool is_root() const noexcept
        {return !is_sentinel() && parent_->is_sentinel();}

    bool is_leaf() const noexcept {return children_.empty();}

    const TokenTrieNode* parent() const {return parent_;}

    const token_type& token() const
    {
        assert(!is_sentinel());
        static token_type emptyToken;
        if (is_root())
            return emptyToken;
        return position_->first;
    }

    key_type key() const
    {
        key_type key;
        const TokenTrieNode* node = this;
        while (!node->is_root())
        {
            key.push_back(node->token());
            node = node->parent();
        }
        std::reverse(key.begin(), key.end());
        return key;
    }

    optional_value& element() {return element_;}

    const optional_value& element() const {return element_;}

    const tree_type& children() const {return children_;}

private:
    using TreeIterator = typename tree_type::iterator;

    tree_type children_;
    optional_value element_;
    TreeIterator position_ = {};
    TokenTrieNode* parent_ = nullptr;

    template <typename, bool> friend class TokenTrieCursor;

    template <typename, typename, typename>
    friend class internal::TokenTrieImpl;
};

//------------------------------------------------------------------------------
template <typename N, bool IsMutable>
class CPPWAMP_API TokenTrieCursor
{
public:
    using node_type = N;
    using key_type = typename N::key_type;
    using key_compare = typename N::key_compare;
    using token_type = typename N::token_type;
    using level_type = typename key_type::size_type;
    using optional_value = typename N::optional_value;
    using value_type = typename N::value_type;
    using reference = typename std::conditional<IsMutable, optional_value&,
                                                const optional_value&>::type;
    using node_pointer = typename std::conditional<IsMutable, node_type*,
                                                   const node_type*>::type;
    using const_iterator = typename node_type::tree_type::const_iterator;
    using iterator =
        typename std::conditional<
            IsMutable,
            typename node_type::tree_type::iterator,
            const_iterator>::type;

    static constexpr bool is_mutable() {return IsMutable;}

    TokenTrieCursor() = default;

    /** Conversion from mutable cursor to const cursor. */
    template <bool RM, typename std::enable_if<!IsMutable && RM, int>::type = 0>
    TokenTrieCursor(const TokenTrieCursor<N, RM>& rhs)
        : parent_(rhs.parent_),
          child_(rhs.child_)
    {}

    /** Assignment from mutable cursor to const cursor. */
    template <bool RM, typename std::enable_if<!IsMutable && RM, int>::type = 0>
    TokenTrieCursor& operator=(const TokenTrieCursor<N, RM>& rhs)
    {
        parent_ = rhs.parent_;
        child_ = rhs.child_;
        return *this;
    }

    explicit operator bool() const {return good();}

    bool good() const {return !at_end() && !at_end_of_level();}

    bool at_end() const {return !parent_ || !parent_->parent();}

    bool at_end_of_level() const
        {return at_end() || child_ == parent_->children().end();}

    bool has_value() const
        {return !at_end_of_level() && childNode().element().has_value();}

    bool token_and_value_equals(const TokenTrieCursor& rhs) const
    {
        if (!good())
            return !rhs.good();

        return rhs.good() && tokensAreEquivalent(token(), rhs.token()) &&
               childNode().element() == rhs.childNode().element();
    }

    bool token_or_value_differs(const TokenTrieCursor& rhs) const
    {
        if (!good())
            return rhs.good();

        return !rhs.good() || tokensAreNotEquivalent(token(), rhs.token()) ||
               childNode().element() != rhs.childNode().element();
    }

    const node_type* parent() const {return parent_;}

    const node_type* child() const
    {
        return child_ == parentNode().children_.end() ? nullptr
                                                      : &(child_->second);
    }

    iterator iter() {return child_;}

    const_iterator iter() const {return child_;}

    // TODO: Read-only map view providing mutable iterators

    iterator begin() {return parentNode().children_.begin();}

    const_iterator begin() const {return parentNode().children_.cbegin();}

    const_iterator cbegin() const {return parentNode().children_.cbegin();}

    iterator end() {return parentNode().children_.end();}

    const_iterator end() const {return parentNode().children_.cend();}

    const_iterator cend() const {return parentNode().children_.cend();}

    iterator lower_bound(const token_type& token)
        {return parentNode().children_.lower_bound(token);}

    const_iterator lower_bound(const token_type& token) const
        {return parentNode().children_.lower_bound(token);}

    key_type key() const {return childNode().key();}

    const token_type& token() const
        {assert(!at_end_of_level()); return child_->first;}

    const optional_value& element() const {return childNode().element();}

    reference element()
        {assert(!at_end_of_level()); return child_->second.element();}

    void advance_depth_first_to_next_node()
    {
        while (!parent_->is_sentinel())
        {
            advanceDepthFirst();
            if (child_ != parent_->children_.end())
                break;
        }
    }

    void advance_depth_first_to_next_element()
    {
        while (!parent_->is_sentinel())
        {
            advanceDepthFirst();
            if (has_value())
                break;
        }
    }

    void advance_to_next_node_in_level()
    {
        assert(!at_end_of_level());
        ++child_;
    }

    void skip_to(iterator iter) {child_ = iter;}

    level_type ascend(level_type level)
    {
        child_ = parent_->position_;
        parent_ = parent_->parent_;
        if (!parent_->is_sentinel())
        {
            assert(level > 0);
            --level;
        }
        return level;
    }

    level_type descend(level_type level)
    {
        assert(good());
        auto& child = child_->second;
        assert(!child.is_leaf());
        parent_ = &child;
        child_ = child.children_.begin();
        return level + 1;
    }

private:
    using NodeRef = typename std::conditional<IsMutable, node_type&,
                                              const node_type&>::type;
    using KeyComp = typename node_type::key_compare;

    static TokenTrieCursor begin(NodeRef rootNode)
    {
        return TokenTrieCursor(&rootNode, rootNode.children_.begin());
    }

    static TokenTrieCursor first(NodeRef rootNode)
    {
        auto cursor = begin(rootNode);
        if (!cursor.at_end_of_level() && !cursor.child()->element().has_value())
            cursor.advance_depth_first_to_next_element();
        return cursor;
    }

    static TokenTrieCursor end(NodeRef sentinelNode)
    {
        return TokenTrieCursor(&sentinelNode, sentinelNode.children_.end());
    }

    static bool tokensAreEquivalent(const token_type& a, const token_type& b)
    {
        key_compare c;
        return !c(a, b) && !c(b, a);
    }

    static bool tokensAreNotEquivalent(const token_type& a, const token_type& b)
    {
        key_compare c;
        return c(a, b) || c(b, a);
    }

    TokenTrieCursor(node_pointer node, iterator iter)
        : parent_(node),
          child_(iter)
    {}

    NodeRef parentNode() const
    {
        assert(parent_ != nullptr);
        return *parent_;
    }

    NodeRef childNode() const
    {
        assert(!at_end_of_level());
        return child_->second;
    }

    void advanceDepthFirst()
    {
        if (child_ != parent_->children_.end())
        {
            if (!child_->second.is_leaf())
            {
                auto& child = child_->second;
                parent_ = &child;
                child_ = child.children_.begin();
            }
            else
            {
                ++child_;
            }
        }
        else if (!parent_->is_sentinel())
        {
            child_ = parent_->position_;
            parent_ = parent_->parent_;
            if (!parent_->is_sentinel())
                ++child_;
            else
                child_ = parent_->children_.end();
        }
    }

    node_pointer parent_ = nullptr;
    iterator child_ = {};

    template <typename, bool> friend class TokenTrieCursor;

    template <typename, typename, typename>
    friend class internal::TokenTrieImpl;

    template <typename TNode, bool L, bool R>
    friend bool operator==(const TokenTrieCursor<TNode, L>& lhs,
                           const TokenTrieCursor<TNode, R>& rhs);

    template <typename TNode, bool L, bool R>
    friend bool operator!=(const TokenTrieCursor<TNode, L>& lhs,
                           const TokenTrieCursor<TNode, R>& rhs);
};

template <typename N, bool L, bool R>
bool operator==(const TokenTrieCursor<N, L>& lhs,
                const TokenTrieCursor<N, R>& rhs)
{
    if (lhs.parent_ == nullptr || rhs.parent_ == nullptr)
        return lhs.parent_ == rhs.parent_;
    return (lhs.parent_ == rhs.parent_) && (lhs.child_ == rhs.child_);
}

template <typename N, bool L, bool R>
bool operator!=(const TokenTrieCursor<N, L>& lhs,
                const TokenTrieCursor<N, R>& rhs)
{
    if (lhs.parent_ == nullptr || rhs.parent_ == nullptr)
        return lhs.parent_ != rhs.parent_;
    return (lhs.parent_ != rhs.parent_) || (lhs.child_ != rhs.child_);
}

} // namespace wamp

#endif // CPPWAMP_TOKENTRIENODE_HPP
