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
    using ValueStorage = S;
    using OptionalValue = TokenTrieOptionalValue<ValueStorage>;
    using Value = typename OptionalValue::value_type;
    using Key = K;
    using Token = typename Key::value_type;
    using Tree = std::map<Token, TokenTrieNode>;
    using TreeIterator = typename Tree::iterator;
    using Level = typename Key::size_type;
    using Size = typename Tree::size_type;
    using Allocator = typename Tree::allocator_type;

    TokenTrieNode() : position_(children_.end()) {}

    template <typename... Us>
    TokenTrieNode(in_place_t, Us&&... args)
        : value_(in_place, std::forward<Us>(args)...)
    {}

    bool isSentinel() const {return parent_ == nullptr;}

    bool isRoot() const {return !isSentinel() && parent_->isSentinel();}

    bool isLeaf() const {return children_.empty();}

    const TokenTrieNode* parent() const {return parent_;}

    const Token& token() const
    {
        assert(!isSentinel());
        static Token emptyToken;
        if (isRoot())
            return emptyToken;
        return position_->first;
    }

    Key key() const
    {
        Key key;
        const TokenTrieNode* node = this;
        while (!node->isRoot())
        {
            key.push_back(node->token());
            node = node->parent();
        }
        std::reverse(key.begin(), key.end());
        return key;
    }

    OptionalValue& value() {return value_;}

    const OptionalValue& value() const {return value_;}

    const Tree& children() const {return children_;}

private:
    Tree children_;
    OptionalValue value_;
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
    using Node = N;
    using Tree = typename Node::Tree;
    using TreeIterator = typename Node::TreeIterator;
    using Key = typename N::Key;
    using Token = typename N::Token;
    using Value = typename N::Value;
    using OptionalValue = typename N::OptionalValue;
    using Level = typename Key::size_type;

    TokenTrieCursor() = default;

    explicit operator bool() const {return good();}

    bool good() const {return !atEnd() && !atEndOfLevel();}

    bool atEnd() const {return !parent_ || !parent_->parent_;}

    bool atEndOfLevel() const
        {return atEnd() || child_ == parent_->children_.end();}

    bool hasValue() const
        {return !atEndOfLevel() && childNode().value_.has_value();}

    template <typename TCursor>
    bool equals(const TCursor& rhs) const
    {
        if (!good())
            return !rhs.good();

        return rhs.good() && token() == rhs.token() &&
               childNode().value() == rhs.childNode().value();
    }

    template <typename TCursor>
    bool differs(const TCursor& rhs) const
    {
        if (!good())
            return rhs.good();

        return !rhs.good() || token() != rhs.token() ||
               childNode().value() != rhs.childNode().value();
    }

    const Node* parent() const {return parent_;}

    const Node* child() const
    {
        return child_ == parentNode().children_.end() ? nullptr
                                                      : &(child_->second);
    }

    Key key() const {return childNode().key();}

    const Token& token() const
        {assert(!atEndOfLevel()); return child_->first;}

    const Value& value() const {return *(childNode().value());}

    Value& value()
        {assert(!atEndOfLevel()); return *(child_->second.value());}

    void advanceToNextTerminal()
    {
        while (!parent_->isSentinel())
        {
            advanceDepthFirst();
            if (hasValue())
                break;
        }
    }

    void advanceToNextNode()
    {
        while (!parent_->isSentinel())
        {
            advanceDepthFirst();
            if (child_ != parent_->children_.end())
                break;
        }
    }

    void findLowerBound(const Key& key)
    {
        findBound(key);
        if (!hasValue())
            advanceToNextTerminal();
    }

    void findUpperBound(const Key& key)
    {
        bool foundExact = findBound(key);
        if (!hasValue() || foundExact)
            advanceToNextTerminal();
    }

    static std::pair<TokenTrieCursor, TokenTrieCursor>
    findEqualRange(Node& rootNode, const Key& key)
    {
        auto lower = begin(rootNode);
        bool foundExact = lower.findBound(key);
        bool nudged = !lower.hasValue();
        if (nudged)
            lower.advanceToNextTerminal();

        TokenTrieCursor upper{lower};
        if (!nudged && foundExact)
            upper.advanceToNextTerminal();
        return {lower, upper};
    }

    Level matchFirst(const Key& key)
    {
        Level level = 0;
        if (key.empty())
        {
            child_ = parent_->children_.end();
        }
        else if (!isMatch(key, 0))
        {
            level = matchNext(key, 0);
        }
        return level;
    }

    Level matchNext(const Key& key, Level level)
    {
        while (!parent_->isSentinel())
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
    TokenTrieCursor(Node& root, TreeIterator iter)
        : parent_(&root),
        child_(iter)
    {}

    static TokenTrieCursor begin(Node& rootNode)
    {
        return TokenTrieCursor(rootNode, rootNode.children_.begin());
    }

    static TokenTrieCursor end(Node& sentinelNode)
    {
        return TokenTrieCursor(sentinelNode, sentinelNode.children_.end());
    }

    const Node& parentNode() const
    {
        assert(parent_ != nullptr);
        return *parent_;
    }

    const Node& childNode() const
    {
        assert(!atEndOfLevel());
        return child_->second;
    }

    void locate(const Key& key)
    {
        assert(!key.empty());
        Node* sentinel = parent_->parent_;
        bool found = true;
        for (Level level = 0; level<key.size(); ++level)
        {
            const auto& token = key[level];
            child_ = parent_->children_.find(token);
            if (atEndOfLevel())
            {
                found = false;
                break;
            }

            if (level < key.size() - 1)
                parent_ = &(child_->second);
        }
        found = found && hasValue();

        if (!found)
        {
            parent_ = sentinel;
            child_ = sentinel->children_.end();
        }
    }

    void advanceToFirstTerminal()
    {
        if (!atEndOfLevel() && !child()->value().has_value())
            advanceToNextTerminal();
    }

    template <typename... Us>
    bool put(bool clobber, Key key, Us&&... args)
    {
        // To avoid dangling link nodes in the event of an exception,
        // build a sub-chain first with the new node, than attach it to the
        // existing tree using move semantics.

        assert(!key.empty());
        const auto tokenCount = key.size();

        // Find existing node from which to possibly attach a sub-chain with
        // the new node.
        Level level = 0;
        for (; level < tokenCount; ++level)
        {
            const auto& token = key[level];
            child_ = parent_->children_.find(token);
            if (child_ == parent_->children_.end())
                break;
            parent_ = &(child_->second);
        }

        // Check if node already exists at the destination level
        // in the existing tree.
        if (level == tokenCount)
        {
            bool placed = false;
            auto& child = child_->second;
            parent_ = child.parent_;
            placed = !hasValue();
            if (placed || clobber)
                child.value_ = Value(std::forward<Us>(args)...);
            return placed;
        }

        // Check if only a single terminal node needs to be added
        assert(level < tokenCount);
        if (tokenCount - level == 1)
        {
            child_ = addTerminal(parent_, key[level],
                                 std::forward<Us>(args)...);
            child_->second.position_ = child_;
            child_->second.parent_ = parent_;
            return true;
        }

        // Build and attach the sub-chain containing the new node.
        Node chain;
        auto token = std::move(key[level]);
        buildChain(&chain, std::move(key), level, std::forward<Us>(args)...);
        child_ = addChain(std::move(token), std::move(chain));
        parent_ = child_->second.parent_;
        return true;
    }

    template <typename... Us>
    TreeIterator addTerminal(Node* node, Token label, Us&&... args)
    {
        auto result = node->children_.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(std::move(label)),
            std::forward_as_tuple(in_place, std::forward<Us>(args)...));
        assert(result.second);
        return result.first;
    }

    template <typename... Us>
    void buildChain(Node* node, Key&& key, Level level, Us&&... args)
    {
        const auto tokenCount = key.size();
        ++level;

        // Add intermediary link nodes
        for (; level < tokenCount - 1; ++level)
        {
            auto iter = buildLink(*node, std::move(key[level]));
            node = &(iter->second);
        }

        // Add terminal node
        assert(level < key.size());
        addTerminal(node, std::move(key[level]), std::forward<Us>(args)...);
    }

    template <typename... Us>
    TreeIterator buildLink(Node& node, Token label)
    {
        auto result = node.children_.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(std::move(label)),
            std::forward_as_tuple());
        assert(result.second);
        return result.first;
    }

    TreeIterator addChain(Token&& label, Node&& chain)
    {
        auto result = parent_->children_.emplace(std::move(label),
                                                 std::move(chain));
        assert(result.second);

        // Traverse down the emplaced chain and set the parent/position
        // fields to their proper values. Better to do this after emplacing
        // the chain to avoid invalid pointers/iterators.
        auto iter = result.first;
        auto node = parent_;
        while (!node->isLeaf())
        {
            Node& child = iter->second;
            child.position_ = iter;
            child.parent_ = node;
            node = &child;
            iter = child.children_.begin();
        }
        return node->position_;
    }

    void eraseFromHere()
    {
        child_->second.value_.reset();

        if (child()->isLeaf())
        {
            // Erase the terminal node, then all obsolete links up the chain
            // until we hit another terminal node or the sentinel.
            while (!hasValue() && !atEnd())
            {
                parent_->children_.erase(child_);
                child_ = parent_->position_;
                parent_ = parent_->parent_;
            }
        }
    }

    void advanceDepthFirst()
    {
        if (child_ != parent_->children_.end())
        {
            if (!child_->second.isLeaf())
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
        else if (!parent_->isSentinel())
        {
            child_ = parent_->position_;
            parent_ = parent_->parent_;
            if (!parent_->isSentinel())
                ++child_;
            else
                child_ = parent_->children_.end();
        }
    }

    bool isMatch(const Key& key, Level level) const
    {
        assert(!key.empty());
        const Level maxLevel = key.size() - 1;
        if ((level != maxLevel) || (child_ == parent_->children_.end()))
            return false;

        // All nodes above the current level are matches. Only the bottom
        // level needs to be checked.
        assert(level < key.size());
        return hasValue() && tokenMatches(key[level]);
    }

    bool tokenMatches(const Token& expectedToken) const
    {
        return child_->first.empty() || child_->first == expectedToken;
    }

    Level findNextMatchCandidate(const Key& key, Level level)
    {
        const Level maxLevel = key.size() - 1;
        if (child_ != parent_->children_.end())
        {
            assert(level < key.size());
            const auto& expectedToken = key[level];
            bool canDescend = !child_->second.isLeaf() && (level < maxLevel) &&
                              tokenMatches(expectedToken);
            if (canDescend)
                level = descend(level);
            else
                findTokenInLevel(expectedToken);
        }
        else if (!parent_->isSentinel())
        {
            level = ascend(level);
            if (!parent_->isSentinel() || child_ != parent_->children_.end())
                findTokenInLevel(key[level]);
        }
        return level;
    }

    Level ascend(Level level)
    {
        child_ = parent_->position_;
        parent_ = parent_->parent_;
        if (!parent_->isSentinel())
        {
            assert(level > 0);
            --level;
        }
        return level;
    }

    Level descend(Level level)
    {
        auto& child = child_->second;
        parent_ = &child;
        child_ = child.children_.begin();
        return level + 1;
    }

    struct Less
    {
        bool operator()(const typename TreeIterator::value_type& kv,
                        const Token& s) const
        {
            return kv.first < s;
        }

        bool operator()(const Token& s,
                        const typename TreeIterator::value_type& kv) const
        {
            return s < kv.first;
        }
    };

    struct LessEqual
    {
        bool operator()(const typename TreeIterator::value_type& kv,
                        const Token& s) const
        {
            return kv.first <= s;
        }

        bool operator()(const Token& s,
                        const typename TreeIterator::value_type& kv) const
        {
            return s <= kv.first;
        }
    };

    void findTokenInLevel(const Token& token)
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

    bool findBound(const Key& key)
    {
        assert(!key.empty());
        const Level maxLevel = key.size() - 1;

        bool foundExact = false;
        for (Level level = 0; level <= maxLevel; ++level)
        {
            const auto& targetToken = key[level];
            child_ = findLowerBoundInNode(*parent_, targetToken);
            if (child_ == parent_->children_.end())
                break;

            if (child_->first != targetToken)
                break;

            if (level < maxLevel)
            {
                if (child_->second.isLeaf() )
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

    static TreeIterator findLowerBoundInNode(Node& n, const Token& token)
    {
        return std::lower_bound(n.children_.begin(), n.children_.end(),
                                token, Less{});
    }

    Node* parent_ = nullptr;
    TreeIterator child_ = {};

    template <typename, typename> friend class internal::TokenTrieImpl;
};

} // namespace wamp

#endif // CPPWAMP_TOKENTRIENODE_HPP
