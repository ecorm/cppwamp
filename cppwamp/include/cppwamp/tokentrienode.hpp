/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TOKENTRIENODE_HPP
#define CPPWAMP_TOKENTRIENODE_HPP

#include <algorithm>
#include <cassert>
#include <map>
#include <tuple>
#include <utility>

#include "api.hpp"

namespace wamp
{

namespace internal { template <typename, typename> class TokenTrieImpl; }

//------------------------------------------------------------------------------
template <typename K, typename T>
class CPPWAMP_API TokenTrieNode
{
public:
    using Value = T;
    using Key = K;
    using Token = typename Key::value_type;
    using Tree = std::map<Token, TokenTrieNode>;
    using TreeIterator = typename Tree::iterator;
    using Level = typename Key::size_type;
    using Size = typename Tree::size_type;
    using Allocator = typename Tree::allocator_type;

    TokenTrieNode() : position_(children_.end()) {}

    template <typename... Us>
    TokenTrieNode(bool isTerminal, Us&&... args)
        : value_(std::forward<Us>(args)...),
          isTerminal_(isTerminal)
    {}

    bool isSentinel() const {return parent_ == nullptr;}

    bool isRoot() const {return !isSentinel() && parent_->isSentinel();}

    bool isLeaf() const {return children_.empty();}

    bool isTerminal() const {return isTerminal_;}

    const TokenTrieNode* parent() const {return parent_;}

    const Token& token() const
    {
        static Token emptyToken;
        if (isRoot())
            return emptyToken;
        return position_->first;
    }

    Key generateKey() const
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

    Value& value()
    {
        assert(isTerminal_);
        return value_;
    }

    const Value& value() const
    {
        assert(isTerminal_);
        return value_;
    }

    const Tree& children() const {return children_;}

    bool operator==(const TokenTrieNode& rhs) const
    {
        if (!isTerminal_)
            return !rhs.isTerminal;
        return rhs.isTerminal && (value_ == rhs.value);
    }

    bool operator!=(const TokenTrieNode& rhs) const
    {
        if (!isTerminal_)
            return rhs.isTerminal;
        return !rhs.isTerminal || (value_ != rhs.value);
    }

private:
    template <typename... Us>
    void setValue(Us&&... args)
    {
        value_ = Value(std::forward<Us>(args)...);
        isTerminal_ = true;
    }

    void clearValue()
    {
        value_ = Value();
        isTerminal_ = false;
    }

    // TODO: Store the value in heap memory to avoid wasting space in
    // non-terminal nodes and to avoid it needing to be default constructible.

    Tree children_;
    Value value_ = {};
    TreeIterator position_ = {};
    TokenTrieNode* parent_ = nullptr;
    bool isTerminal_ = false;

    template <typename, typename> friend class TokenTrieCursor;
    template <typename, typename> friend class internal::TokenTrieImpl;
};

//------------------------------------------------------------------------------
template <typename K, typename T>
class CPPWAMP_API TokenTrieCursor
{
public:
    using Node = TokenTrieNode<K, T>;
    using Tree = typename Node::Tree;
    using TreeIterator = typename Node::TreeIterator;
    using Key = K;
    using Token = typename Key::value_type;
    using Value = typename Node::Value;
    using StringType = typename Key::value_type;
    using Level = typename Key::size_type;

    TokenTrieCursor() = default;

    explicit operator bool() const
    {return parent_ == nullptr || parent_->parent_ != nullptr;}

    const Node* parent() const {return parent_;}

    const Node* child() const
    {
        return child_ == parentNode().children_.end() ? nullptr
                                                      : &(child_->second);
    }

    const Token& childToken() const
        {assert(!atEndOfLevel()); return child_->first;}

    const Value& childValue() const
        {assert(!atEndOfLevel()); return child_->second.value();}

    Value& childValue()
        {assert(!atEndOfLevel()); return child_->second.value();}

    bool atEndOfLevel() const
        {return !parent_ || child_ == parent_->children_.end();}

    void advanceToNextTerminal()
    {
        while (!parent_->isSentinel())
        {
            advanceDepthFirst();
            if (!atEndOfLevel() && child()->isTerminal())
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
        if (atEndOfLevel() || !child()->isTerminal())
            advanceToNextTerminal();
    }

    void findUpperBound(const Key& key)
    {
        bool foundExact = findBound(key);
        if (atEndOfLevel() || !child()->isTerminal() || foundExact)
            advanceToNextTerminal();
    }

    static std::pair<TokenTrieCursor, TokenTrieCursor>
    findEqualRange(Node& rootNode, const Key& key)
    {
        auto lower = begin(rootNode);
        bool foundExact = lower.findBound(key);
        bool nudged = lower.atEndOfLevel() || !lower.child()->isTerminal();
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

    void locate(const Key& key)
    {
        assert(!key.empty());
        Node* sentinel = parent_->parent_;
        bool found = true;
        for (Level level = 0; level<key.size(); ++level)
        {
            const auto& token = key[level];
            child_ = parent_->children_.find(token);
            if (child_ == parent_->children_.end())
            {
                found = false;
                break;
            }

            if (level < key.size() - 1)
                parent_ = &(child_->second);
        }
        found = found && child_->second.isTerminal();

        if (!found)
        {
            parent_ = sentinel;
            child_ = sentinel->children_.end();
        }
    }

    void advanceToFirstTerminal()
    {
        if (!atEndOfLevel() && !child()->isTerminal())
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
            placed = !child.isTerminal();
            if (placed || clobber)
                child.setValue(std::forward<Us>(args)...);
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
            std::forward_as_tuple(true, std::forward<Us>(args)...));
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
            std::forward_as_tuple(false));
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
        if (child()->isLeaf())
        {
            child_->second.isTerminal_ = false;
            // Erase the terminal node, then all obsolete links up the chain
            // until we hit another terminal node or the sentinel.
            while (!child()->isTerminal() && !parent()->isSentinel())
            {
                parent_->children_.erase(child_);
                child_ = parent_->position_;
                parent_ = parent_->parent_;
            }
        }
        else
        {
            // The terminal node to be erased has children, so we must
            // preserve it and only clear its value.
            child_->second.clearValue();
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
        return child_->second.isTerminal() && tokenMatches(key[level]);
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

    static TreeIterator findLowerBoundInNode(Node& n, const StringType& token)
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
