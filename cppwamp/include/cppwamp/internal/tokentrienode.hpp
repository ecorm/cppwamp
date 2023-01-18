/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_TOKENTRIENODE_HPP
#define CPPWAMP_INTERNAL_TOKENTRIENODE_HPP

#include <algorithm>
#include <cassert>
#include <map>
#include <tuple>
#include <utility>

#include "../api.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
template <typename K, typename T>
struct CPPWAMP_HIDDEN TokenTrieNode
{
    using Value = T;
    using Key = K;
    using Token = typename Key::value_type;
    using Tree = std::map<Token, TokenTrieNode>;
    using TreeIterator = typename Tree::iterator;
    using Level = typename Key::size_type;
    using Size = typename Tree::size_type;
    using Allocator = typename Tree::allocator_type;

    TokenTrieNode() : position(children.end()) {}

    template <typename... Us>
    TokenTrieNode(bool isTerminal, Us&&... args)
        : value(std::forward<Us>(args)...),
          isTerminal(isTerminal)
    {}

    template <typename... Us>
    TreeIterator addTerminal(Token label, Us&&... args)
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
        TokenTrieNode* node = this;
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

    TreeIterator addChain(Token&& label, TokenTrieNode&& chain)
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
            TokenTrieNode& child = iter->second;
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

    Token token() const
    {
        Token s;
        if (!isRoot())
            s = position->first;
        return s;
    }

    Key generateKey() const
    {
        Key key;
        const TokenTrieNode* node = this;
        while (!node->isRoot())
        {
            key.push_back(node->position->first);
            node = node->parent;
        }
        std::reverse(key.begin(), key.end());
        return key;
    }

    bool operator==(const TokenTrieNode& rhs) const
    {
        if (!isTerminal)
            return !rhs.isTerminal;
        return rhs.isTerminal && (value == rhs.value);
    }

    bool operator!=(const TokenTrieNode& rhs) const
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
    TokenTrieNode* parent = nullptr;
    bool isTerminal = false;

private:
    template <typename... Us>
    TreeIterator buildLink(Token label)
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

} // namespace wamp

#endif // CPPWAMP_INTERNAL_TOKENTRIENODE_HPP
