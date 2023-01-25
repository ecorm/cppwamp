/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TOKENTRIENODE_HPP
#define CPPWAMP_TOKENTRIENODE_HPP

#include <algorithm>
#include <cassert>
#include <initializer_list>
#include <map>
#include <tuple>
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
    template <typename, typename, typename> class TokenTrieImpl;
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
template <typename K, typename S>
class CPPWAMP_API TokenTrieNode
{
public:
    using value_storage = S;
    using optional_value = TokenTrieOptionalValue<value_storage>;
    using value_type = typename optional_value::value_type;
    using key_type = K;
    using token_type = typename key_type::value_type;
    using tree_type = std::map<token_type, TokenTrieNode>;
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
    using token_type = typename N::token_type;
    using size_type = typename key_type::size_type;
    using optional_value = typename N::optional_value;
    using value_type = typename N::value_type;
    using reference = typename std::conditional<IsMutable, optional_value&,
                                                const optional_value&>::type;
    using node_pointer = typename std::conditional<IsMutable, node_type*,
                                                   const node_type*>::type;

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

    bool equals(const TokenTrieCursor& rhs) const
    {
        if (!good())
            return !rhs.good();

        return rhs.good() && token() == rhs.token() &&
               childNode().value() == rhs.childNode().value();
    }

    bool differs(const TokenTrieCursor& rhs) const
    {
        if (!good())
            return rhs.good();

        return !rhs.good() || token() != rhs.token() ||
               childNode().value() != rhs.childNode().value();
    }

    const node_type* parent() const {return parent_;}

    const node_type* child() const
    {
        return child_ == parentNode().children_.end() ? nullptr
                                                      : &(child_->second);
    }

    key_type key() const {return childNode().key();}

    const token_type& token() const
        {assert(!at_end_of_level()); return child_->first;}

    const optional_value& element() const {return childNode().element();}

    reference element()
        {assert(!at_end_of_level()); return child_->second.element();}

    void advance_to_next_element()
    {
        while (!parent_->is_sentinel())
        {
            advanceDepthFirst();
            if (has_value())
                break;
        }
    }

    void advance_to_next_node()
    {
        while (!parent_->is_sentinel())
        {
            advanceDepthFirst();
            if (child_ != parent_->children_.end())
                break;
        }
    }

    size_type match_first(const key_type& key)
    {
        size_type level = 0;
        if (key.empty())
        {
            child_ = parent_->children_.end();
        }
        else if (!isMatch(key, 0))
        {
            level = match_next(key, 0);
        }
        return level;
    }

    size_type match_next(const key_type& key, size_type level)
    {
        while (!parent_->is_sentinel())
        {
            level = findNextMatchCandidate(key, level);
            if (isMatch(key, level))
                break;
        }
        return level;
    }

private:
    using Tree = typename node_type::tree_type;
    using TreeIterator =
        typename std::conditional<IsMutable,
                                  typename Tree::iterator,
                                  typename Tree::const_iterator>::type;
    using NodeRef = typename std::conditional<IsMutable, node_type&,
                                              const node_type&>::type;

    TokenTrieCursor(node_pointer root, TreeIterator iter)
        : parent_(root),
          child_(iter)
    {}

    static TokenTrieCursor begin(NodeRef rootNode)
    {
        return TokenTrieCursor(&rootNode, rootNode.children_.begin());
    }

    static TokenTrieCursor first(NodeRef rootNode)
    {
        auto cursor = begin(rootNode);
        cursor.advanceToFirstValue();
        return cursor;
    }

    static TokenTrieCursor end(NodeRef sentinelNode)
    {
        return TokenTrieCursor(&sentinelNode, sentinelNode.children_.end());
    }

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

    void advanceToFirstValue()
    {
        if (!at_end_of_level() && !child()->element().has_value())
            advance_to_next_element();
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

    bool isMatch(const key_type& key, size_type level) const
    {
        assert(!key.empty());
        const size_type maxLevel = key.size() - 1;
        if ((level != maxLevel) || (child_ == parent_->children_.end()))
            return false;

        // All nodes above the current level are matches. Only the bottom
        // level needs to be checked.
        assert(level < key.size());
        return has_value() && tokenMatches(key[level]);
    }

    bool tokenMatches(const token_type& expectedToken) const
    {
        return child_->first.empty() || child_->first == expectedToken;
    }

    size_type findNextMatchCandidate(const key_type& key, size_type level)
    {
        const size_type maxLevel = key.size() - 1;
        if (child_ != parent_->children_.end())
        {
            assert(level < key.size());
            const auto& expectedToken = key[level];
            bool canDescend = !child_->second.is_leaf() && (level < maxLevel) &&
                              tokenMatches(expectedToken);
            if (canDescend)
                level = descend(level);
            else
                findTokenInLevel(expectedToken);
        }
        else if (!parent_->is_sentinel())
        {
            level = ascend(level);
            if (!parent_->is_sentinel() || child_ != parent_->children_.end())
                findTokenInLevel(key[level]);
        }
        return level;
    }

    size_type ascend(size_type level)
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

    size_type descend(size_type level)
    {
        auto& child = child_->second;
        parent_ = &child;
        child_ = child.children_.begin();
        return level + 1;
    }

    struct Less
    {
        bool operator()(const typename TreeIterator::value_type& kv,
                        const token_type& s) const
        {
            return kv.first < s;
        }

        bool operator()(const token_type& s,
                        const typename TreeIterator::value_type& kv) const
        {
            return s < kv.first;
        }
    };

    void findTokenInLevel(const token_type& token)
    {
        if (child_ == parent_->children_.begin())
        {
            child_ = std::lower_bound(++child_, parent_->children_.end(),
                                      token, Less{});
            if (child_ != parent_->children_.end() && child_->first != token)
                child_ = parent_->children_.end();
        }
        else
        {
            child_ = parent_->children_.end();
        }
    }

    node_pointer parent_ = nullptr;
    TreeIterator child_ = {};

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
