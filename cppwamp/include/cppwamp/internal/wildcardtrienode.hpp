/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_WILCARDTRIENODE_HPP
#define CPPWAMP_INTERNAL_WILCARDTRIENODE_HPP

#include <algorithm>
#include <cassert>
#include <map>
#include <tuple>
#include <utility>

#include "../api.hpp"
#include "../uri.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
template <typename T>
struct CPPWAMP_HIDDEN WildcardTrieNode
{
    using Value = T;
    using Key = SplitUri;
    using Atom = typename Key::value_type;
    using Tree = std::map<Atom, WildcardTrieNode>;
    using TreeIterator = typename Tree::iterator;
    using Level = typename Key::size_type;
    using Size = typename Tree::size_type;
    using Allocator = typename Tree::allocator_type;

    WildcardTrieNode() : position(children.end()) {}

    template <typename... Us>
    WildcardTrieNode(bool isTerminal, Us&&... args)
        : value(std::forward<Us>(args)...),
          isTerminal(isTerminal)
    {}

    template <typename... Us>
    TreeIterator addTerminal(Atom label, Us&&... args)
    {
        auto result = children.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(std::move(label)),
            std::forward_as_tuple(true, std::forward<Us>(args)...));
        assert(result.second);
        return result.first;
    }

    template <typename... Us>
    void buildChain(Key&& key, Level level, Us&&... args)
    {
        const auto tokenCount = key.size();
        WildcardTrieNode* node = this;
        ++level;

        // Add intermediary link nodes
        for (; level < tokenCount - 1; ++level)
        {
            auto iter = node->buildLink(std::move(key[level]));
            node = &(iter->second);
        }

        // Add terminal node
        assert(level < key.size());
        node->addTerminal(std::move(key[level]), std::forward<Us>(args)...);
    }

    TreeIterator addChain(Atom&& label, WildcardTrieNode&& chain)
    {
        auto result = children.emplace(std::move(label), std::move(chain));
        assert(result.second);

        // Traverse down the emplaced chain and set the parent/position
        // fields to their proper values. Better to do this after emplacing
        // the chain to avoid invalid pointers/iterators.
        auto iter = result.first;
        auto node = this;
        while (!node->isLeaf())
        {
            WildcardTrieNode& child = iter->second;
            child.position = iter;
            child.parent = node;
            node = &child;
            iter = child.children.begin();
        }
        return node->position;
    }

    template <typename... Us>
    void setValue(Us&&... args)
    {
        value = Value(std::forward<Us>(args)...);
        isTerminal = true;
    }

    void clear()
    {
        value = Value();
        isTerminal = false;
    }

    bool isSentinel() const {return parent == nullptr;}

    bool isRoot() const {return !isSentinel() && parent->isSentinel();}

    bool isLeaf() const {return children.empty();}

    Key generateKey() const
    {
        Key key;
        const WildcardTrieNode* node = this;
        while (!node->isRoot())
        {
            key.emplace_back(node->position->first);
            node = node->parent;
        }
        std::reverse(key.begin(), key.end());
        return key;
    }

    bool operator==(const WildcardTrieNode& rhs) const
    {
        if (!isTerminal)
            return !rhs.isTerminal;
        return rhs.isTerminal && (value == rhs.value);
    }

    bool operator!=(const WildcardTrieNode& rhs) const
    {
        if (!isTerminal)
            return rhs.isTerminal;
        return !rhs.isTerminal || (value != rhs.value);
    }

    // TODO: Use std::optional (or a pre-C++17 surrogate) for the value
    // to avoid it needing to be default constructible.

    Tree children;
    Value value = {};
    TreeIterator position = {};
    WildcardTrieNode* parent = nullptr;
    bool isTerminal = false;

private:
    template <typename... Us>
    TreeIterator buildLink(Atom label)
    {
        auto result = children.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(std::move(label)),
            std::forward_as_tuple(false));
        assert(result.second);
        return result.first;
    }
};


//------------------------------------------------------------------------------
template <typename T>
class CPPWAMP_HIDDEN WildcardTrieCursor
{
public:
    using Node = WildcardTrieNode<T>;
    using TreeIterator = typename Node::TreeIterator;
    using Key = SplitUri;
    using Value = typename Node::Value;
    using StringType = typename Key::value_type;
    using Level = typename Key::size_type;

    static WildcardTrieCursor begin(Node& rootNode)
    {
        return WildcardTrieCursor(rootNode, rootNode.children.begin());
    }

    static WildcardTrieCursor end(Node& sentinelNode)
    {
        return WildcardTrieCursor(sentinelNode, sentinelNode.children.end());
    }

    WildcardTrieCursor() = default;

    void locate(const Key& key)
    {
        assert(!key.empty());
        Node* sentinel = node->parent;
        bool found = true;
        for (Level level = 0; level<key.size(); ++level)
        {
            const auto& label = key[level];
            iter = node->children.find(label);
            if (iter == node->children.end())
            {
                found = false;
                break;
            }

            if (level < key.size() - 1)
                node = &(iter->second);
        }
        found = found && iter->second.isTerminal;

        if (!found)
        {
            node = sentinel;
            iter = sentinel->children.end();
        }
    }

    Key generateKey() const
    {
        if (node == nullptr || iter == node->children.end())
            return {};
        return iter->second.generateKey();
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
            const auto& label = key[level];
            iter = node->children.find(label);
            if (iter == node->children.end())
                break;
            node = &(iter->second);
        }

        // Check if node already exists at the destination level
        // in the existing tree.
        if (level == tokenCount)
        {
            bool placed = false;
            auto& child = iter->second;
            node = child.parent;
            placed = !child.isTerminal;
            if (placed || clobber)
                child.setValue(std::forward<Us>(args)...);
            return placed;
        }

        // Check if only a single terminal node needs to be added
        assert(level < tokenCount);
        if (tokenCount - level == 1)
        {
            iter = node->addTerminal(key[level], std::forward<Us>(args)...);
            iter->second.position = iter;
            iter->second.parent = node;
            return true;
        }

        // Build and attach the sub-chain containing the new node.
        Node chain;
        auto label = std::move(key[level]);
        chain.buildChain(std::move(key), level, std::forward<Us>(args)...);
        iter = node->addChain(std::move(label), std::move(chain));
        node = iter->second.parent;
        return true;
    }

    void eraseFromHere()
    {
        iter->second.isTerminal = false;
        if (iter->second.isLeaf())
        {
            // Erase the terminal node, then all obsolete links up the chain
            // until we hit another terminal node or the sentinel.
            while (!iter->second.isTerminal && !node->isSentinel())
            {
                node->children.erase(iter);
                iter = node->position;
                node = node->parent;
            }
        }
        else
        {
            // The terminal node to be erased has children, so we must
            // preserve it and only clear its value.
            iter->second.value = Value();
        }
    }

    void advanceToFirstTerminal()
    {
        if (!isTerminal())
            advanceToNextTerminal();
    }

    void advanceToNextTerminal()
    {
        while (!isSentinel())
        {
            advanceDepthFirst();
            if (isTerminal())
                break;
        }
    }

    void advanceToNextNode()
    {
        while (!isSentinel())
        {
            advanceDepthFirst();
            if (iter != node->children.end())
                break;
        }
    }

    void findLowerBound(const Key& key)
    {
        findBound(key);
        if (!iter->second.isTerminal)
            advanceToNextTerminal();
    }

    void findUpperBound(const Key& key)
    {
        bool foundExact = findBound(key);
        if (!iter->second.isTerminal || foundExact)
            advanceToNextTerminal();
    }

    static std::pair<WildcardTrieCursor, WildcardTrieCursor>
    findEqualRange(Node& rootNode, const Key& key)
    {
        auto lower = begin(rootNode);
        bool foundExact = lower.findBound(key);
        bool isTerminal = lower.iter->second.isTerminal;
        if (!isTerminal)
            lower.advanceToNextTerminal();

        WildcardTrieCursor upper{lower};
        if (isTerminal && foundExact)
            upper.advanceToNextTerminal();
        return {lower, upper};
    }

    Level matchFirst(const Key& key)
    {
        Level level = 0;
        if (key.empty())
        {
            iter = node->children.end();
        }
        else if (!isMatch(key, 0))
        {
            level = matchNext(key, 0);
        }
        return level;
    }

    Level matchNext(const Key& key, Level level)
    {
        while (!isSentinel())
        {
            level = findNextMatchCandidate(key, level);
            if (isMatch(key, level))
                break;
        }
        return level;
    }

    bool isSentinel() const {return node->parent == nullptr;}

    bool operator==(const WildcardTrieCursor& rhs) const
    {
        return (node == rhs.node) && (iter == rhs.iter);
    }

    bool operator!=(const WildcardTrieCursor& rhs) const
    {
        return (node != rhs.node) || (iter != rhs.iter);
    }

    Node* node = nullptr;
    TreeIterator iter = {};

private:
    WildcardTrieCursor(Node& root, TreeIterator iter)
        : node(&root),
          iter(iter)
    {}

    bool isTerminal() const
    {
        return (iter != node->children.end()) && iter->second.isTerminal;
    }

    void advanceDepthFirst()
    {
        if (iter != node->children.end())
        {
            if (!iter->second.isLeaf())
            {
                auto& child = iter->second;
                node = &child;
                iter = child.children.begin();
            }
            else
            {
                ++iter;
            }
        }
        else if (!node->isSentinel())
        {
            iter = node->position;
            node = node->parent;
            if (!node->isSentinel())
                ++iter;
            else
                iter = node->children.end();
        }
    }

    bool isMatch(const Key& key, Level level) const
    {
        assert(!key.empty());
        const Level maxLevel = key.size() - 1;
        if ((level != maxLevel) || (iter == node->children.end()))
            return false;

        // All nodes above the current level are matches. Only the bottom
        // level needs to be checked.
        assert(level < key.size());
        return iter->second.isTerminal && labelMatches(key[level]);
    }

    bool labelMatches(const StringType& expectedLabel) const
    {
        return iter->first.empty() || iter->first == expectedLabel;
    }

    Level findNextMatchCandidate(const Key& key, Level level)
    {
        const Level maxLevel = key.size() - 1;
        if (iter != node->children.end())
        {
            assert(level < key.size());
            const auto& expectedLabel = key[level];
            bool canDescend = !iter->second.isLeaf() && (level < maxLevel) &&
                              labelMatches(expectedLabel);
            if (canDescend)
                level = descend(level);
            else
                findLabelInLevel(expectedLabel);
        }
        else if (!isSentinel())
        {
            level = ascend(level);
            if (!isSentinel() || iter != node->children.end())
                findLabelInLevel(key[level]);
        }
        return level;
    }

    Level ascend(Level level)
    {
        iter = node->position;
        node = node->parent;
        if (!isSentinel())
        {
            assert(level > 0);
            --level;
        }
        return level;
    }

    Level descend(Level level)
    {
        auto& child = iter->second;
        node = &child;
        iter = child.children.begin();
        return level + 1;
    }

    struct Less
    {
        bool operator()(const typename TreeIterator::value_type& kv,
                        const StringType& s) const
        {
            return kv.first < s;
        }

        bool operator()(const StringType& s,
                        const typename TreeIterator::value_type& kv) const
        {
            return s < kv.first;
        }
    };

    struct LessEqual
    {
        bool operator()(const typename TreeIterator::value_type& kv,
                        const StringType& s) const
        {
            return kv.first <= s;
        }

        bool operator()(const StringType& s,
                        const typename TreeIterator::value_type& kv) const
        {
            return s <= kv.first;
        }
    };

    void findLabelInLevel(const StringType& label)
    {
        if (iter == node->children.begin())
        {
            iter = std::lower_bound(++iter, node->children.end(),
                                    label, Less{});
            if (iter != node->children.end() && iter->first != label)
                iter = node->children.end();
        }
        else
        {
            iter = node->children.end();
        }
    }

    bool findBound(const Key& key)
    {
        assert(!key.empty());
        const Level maxLevel = key.size() - 1;

        bool foundExact = false;
        for (Level level = 0; level <= maxLevel; ++level)
        {
            const auto& targetLabel = key[level];
            iter = findLowerBoundInNode(*node, targetLabel);
            if (iter == node->children.end())
                break;

            if (iter->first != targetLabel)
                break;

            if (level < maxLevel)
            {
                if (iter->second.isLeaf() )
                {
                    ++iter;
                    break;
                }
                node = &(iter->second);
                iter = node->children.begin();
            }
            else
            {
                foundExact = true;
            }
        }

        return foundExact;
    }

    static TreeIterator findLowerBoundInNode(Node& n, const StringType& label)
    {
        return std::lower_bound(n.children.begin(), n.children.end(),
                                label, Less{});
    }
};

template <typename, bool> class WildcardTrieIterator;

//------------------------------------------------------------------------------
struct CPPWAMP_HIDDEN WildcardTrieIteratorAccess
{
    template <typename I>
    static const WildcardTrieCursor<typename I::value_type>&
    cursor(const I& iterator)
    {
        return iterator.cursor_;
    }

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

} // namespace wamp

#endif // CPPWAMP_INTERNAL_WILCARDTRIENODE_HPP
