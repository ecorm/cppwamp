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
        : value_(in_place, std::forward<Us>(args)...)
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

    optional_value& value() {return value_;}

    const optional_value& value() const {return value_;}

    const tree_type& children() const {return children_;}

private:
    using TreeIterator = typename tree_type::iterator;

    tree_type children_;
    optional_value value_;
    TreeIterator position_ = {};
    TokenTrieNode* parent_ = nullptr;

    template <typename> friend class TokenTrieCursor;
    template <typename, typename, typename> friend class internal::TokenTrieImpl;
};

//------------------------------------------------------------------------------
template <typename N>
class CPPWAMP_API TokenTrieCursor
{
public:
    using node_type = N;
    using key_type = typename N::key_type;
    using token_type = typename N::token_type;
    using value_type = typename N::value_type;
    using size_type = typename key_type::size_type;

    TokenTrieCursor() = default;

    explicit operator bool() const {return good();}

    bool good() const {return !at_end() && !at_end_of_level();}

    bool at_end() const {return !parent_ || !parent_->parent_;}

    bool at_end_of_level() const
        {return at_end() || child_ == parent_->children_.end();}

    bool has_value() const
        {return !at_end_of_level() && childNode().value_.has_value();}

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

    const value_type& value() const {return *(childNode().value());}

    value_type& value()
        {assert(!at_end_of_level()); return *(child_->second.value());}

    void advance_to_next_value()
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

    void lower_bound(const key_type& key)
    {
        findBound(key);
        if (!has_value())
            advance_to_next_value();
    }

    void upper_bound(const key_type& key)
    {
        bool foundExact = findBound(key);
        if (!has_value() || foundExact)
            advance_to_next_value();
    }

    static std::pair<TokenTrieCursor, TokenTrieCursor>
    equal_range(node_type& root_node, const key_type& key)
    {
        auto lower = begin(root_node);
        bool foundExact = lower.findBound(key);
        bool nudged = !lower.has_value();
        if (nudged)
            lower.advance_to_next_value();

        TokenTrieCursor upper{lower};
        if (!nudged && foundExact)
            upper.advance_to_next_value();
        return {lower, upper};
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

    bool operator==(const TokenTrieCursor& rhs) const
    {
        if (parent_ == nullptr || rhs.parent_ == nullptr)
            return parent_ == rhs.parent_;
        return (parent_ == rhs.parent_) && (child_ == rhs.child_);
    }

    bool operator!=(const TokenTrieCursor& rhs) const
    {
        if (parent_ == nullptr || rhs.parent_ == nullptr)
            return parent_ != rhs.parent_;
        return (parent_ != rhs.parent_) || (child_ != rhs.child_);
    }

private:
    using Tree = typename node_type::tree_type;
    using TreeIterator = typename Tree::iterator;

    TokenTrieCursor(node_type& root, TreeIterator iter)
        : parent_(&root),
        child_(iter)
    {}

    static TokenTrieCursor begin(node_type& rootNode)
    {
        return TokenTrieCursor(rootNode, rootNode.children_.begin());
    }

    static TokenTrieCursor end(node_type& sentinelNode)
    {
        return TokenTrieCursor(sentinelNode, sentinelNode.children_.end());
    }

    const node_type& parentNode() const
    {
        assert(parent_ != nullptr);
        return *parent_;
    }

    const node_type& childNode() const
    {
        assert(!at_end_of_level());
        return child_->second;
    }

    void locate(const key_type& key)
    {
        assert(!key.empty());
        node_type* sentinel = parent_->parent_;
        bool found = true;
        for (size_type level = 0; level<key.size(); ++level)
        {
            const auto& token = key[level];
            child_ = parent_->children_.find(token);
            if (at_end_of_level())
            {
                found = false;
                break;
            }

            if (level < key.size() - 1)
                parent_ = &(child_->second);
        }
        found = found && has_value();

        if (!found)
        {
            parent_ = sentinel;
            child_ = sentinel->children_.end();
        }
    }

    void advanceToFirstValue()
    {
        if (!at_end_of_level() && !child()->value().has_value())
            advance_to_next_value();
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

    struct LessEqual
    {
        bool operator()(const typename TreeIterator::value_type& kv,
                        const token_type& s) const
        {
            return kv.first <= s;
        }

        bool operator()(const token_type& s,
                        const typename TreeIterator::value_type& kv) const
        {
            return s <= kv.first;
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

    bool findBound(const key_type& key)
    {
        assert(!key.empty());
        const size_type maxLevel = key.size() - 1;

        bool foundExact = false;
        for (size_type level = 0; level <= maxLevel; ++level)
        {
            const auto& targetToken = key[level];
            child_ = findLowerBoundInNode(*parent_, targetToken);
            if (child_ == parent_->children_.end())
                break;

            if (child_->first != targetToken)
                break;

            if (level < maxLevel)
            {
                if (child_->second.is_leaf() )
                {
                    ++child_;
                    break;
                }
                parent_ = &(child_->second);
                child_ = parent_->children_.begin();
            }
            else
            {
                foundExact = true;
            }
        }

        return foundExact;
    }

    static TreeIterator findLowerBoundInNode(node_type& n, const token_type& token)
    {
        return std::lower_bound(n.children_.begin(), n.children_.end(),
                                token, Less{});
    }

    node_type* parent_ = nullptr;
    TreeIterator child_ = {};

    template <typename, typename, typename> friend class internal::TokenTrieImpl;
};

} // namespace wamp

#endif // CPPWAMP_TOKENTRIENODE_HPP
