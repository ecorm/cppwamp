/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_TOKENTRIEIMPL_HPP
#define CPPWAMP_INTERNAL_TOKENTRIEIMPL_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains the TokenTrie template class. */
//------------------------------------------------------------------------------

#include <cassert>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include "../api.hpp"
#include "../tokentrienode.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
template <typename K, typename T, typename P>
class CPPWAMP_HIDDEN TokenTrieImpl
{
public:
    using Key = K;
    using Value = T;
    using Policy = P;
    using ValueStorage = typename P::value_storage;
    using Node = TokenTrieNode<Key, ValueStorage>;
    using Size = typename Node::tree_type::size_type;
    using Cursor = TokenTrieCursor<Node>;

    TokenTrieImpl() {}

    TokenTrieImpl(const TokenTrieImpl& rhs)
        : size_(rhs.size_)
    {
        if (rhs.root_)
        {
            root_.reset(new Node(*rhs.root_));
            root_->parent_ = &sentinel_;
            scanTree();
        }
    }

    TokenTrieImpl(TokenTrieImpl&& rhs) noexcept {moveFrom(rhs);}

    TokenTrieImpl& operator=(const TokenTrieImpl& rhs)
    {
        // Do nothing for self-assignment to enfore the invariant that
        // the RHS iterators remain valid.
        if (&rhs != this)
        {
            TokenTrieImpl temp(rhs);
            (*this) = std::move(temp);
        }
        return *this;
    }

    TokenTrieImpl& operator=(TokenTrieImpl&& rhs) noexcept
    {
        // Do nothing for self-move-assignment to avoid invalidating iterators.
        if (&rhs != this)
            moveFrom(rhs);
        return *this;
    }

    Cursor rootCursor()
    {
        assert(root_ != nullptr);
        return Cursor::begin(*root_);
    }

    Cursor rootCursor() const
    {
        assert(root_ != nullptr);
        return Cursor::begin(const_cast<Node&>(*root_));
    }

    Cursor firstValueCursor()
    {
        if (empty())
            return sentinelCursor();
        auto cursor = rootCursor();
        cursor.advanceToFirstValue();
        return cursor;
    }

    Cursor firstValueCursor() const
    {
        return const_cast<TokenTrieImpl&>(*this).firstValueCursor();
    }

    Cursor sentinelCursor()
    {
        return Cursor::end(sentinel_);
    }

    Cursor sentinelCursor() const
    {
        return Cursor::end(const_cast<Node&>(sentinel_));
    }

    Cursor locate(const Key& key)
    {
        if (empty() || key.empty())
            return sentinelCursor();
        auto cursor = rootCursor();
        cursor.locate(key);
        return cursor;
    }

    Cursor locate(const Key& key) const
    {
        return const_cast<TokenTrieImpl&>(*this).locate(key);
    }

    Cursor lowerBound(const Key& key) const
    {
        if (empty() || key.empty())
            return sentinelCursor();
        auto cursor = rootCursor();
        cursor.lower_bound(key);
        return cursor;
    }

    Cursor upperBound(const Key& key) const
    {
        if (empty() || key.empty())
            return sentinelCursor();
        auto cursor = rootCursor();
        cursor.upper_bound(key);
        return cursor;
    }

    std::pair<Cursor, Cursor> equalRange(const Key& key) const
    {
        if (empty() || key.empty())
            return {sentinelCursor(), sentinelCursor()};
        return Cursor::equal_range(*root_, key);
    }

    Size empty() const noexcept {return size_ == 0;}

    Size size() const noexcept {return size_;}

    void clear() noexcept
    {
        if (root_)
            root_->children_.clear();
        size_ = 0;
    }

    template <typename... Us>
    std::pair<Cursor, bool> put(bool clobber, Key key, Us&&... args)
    {
        if (key.empty())
            return {sentinelCursor(), false};

        if (!root_)
        {
            root_.reset(new Node);
            root_->parent_ = &sentinel_;
        }

        auto result = upsert(clobber, std::move(key),
                             std::forward<Us>(args)...);
        if (result.second)
            ++size_;
        return result;
    }

    Cursor erase(Cursor pos)
    {
        auto cursor = pos;
        assert(bool(cursor));
        pos.advance_to_next_value();

        cursor.child_->second.value_.reset();
        if (cursor.child()->is_leaf())
        {
            // Erase the value node, then all obsolete links up the chain
            // until we hit another value node or the sentinel.
            while (!cursor.has_value() && !cursor.at_end())
            {
                cursor.parent_->children_.erase(cursor.child_);
                cursor.child_ = cursor.parent_->position_;
                cursor.parent_ = cursor.parent_->parent_;
            }
        }

        --size_;
        return pos;
    }

    void swap(TokenTrieImpl& other) noexcept
    {
        root_.swap(other.root_);
        std::swap(size_, other.size_);
        if (root_)
            root_->parent_ = &sentinel_;
        if (other.root_)
            other.root_->parent_ = &other.sentinel_;
    }

    template <typename TOther>
    bool equals(const TOther& rhs) const noexcept
    {
        if (empty() || rhs.empty())
            return empty() == rhs.empty();

        auto curA = rootCursor();
        auto curB = rhs.rootCursor();
        while (!curA.at_end())
        {
            if (curA != curB)
                return false;
            curA.advance_to_next_node();
            curB.advance_to_next_node();
        }
        return curB.at_end();
    }

    template <typename TOther>
    bool differs(const TOther& rhs) const noexcept
    {
        if (empty() || rhs.empty())
            return empty() != rhs.empty();

        auto curA = rootCursor();
        auto curB = rhs.rootCursor();
        while (!curA.at_end())
        {
            if (curB.at_end())
                return true;
            if (curA != curB)
                return true;
            curA.advance_to_next_node();
            curB.advance_to_next_node();
        }
        return !curB.at_end();
    }

private:
    using Tree = typename Node::tree_type;
    using TreeIterator = typename Tree::iterator;
    using Token = typename Node::token_type;

    void moveFrom(TokenTrieImpl& rhs) noexcept
    {
        root_.swap(rhs.root_);
        size_ = rhs.size_;
        rhs.size_ = 0;
        if (root_)
            root_->parent_ = &sentinel_;
    }

    void scanTree()
    {
        root_->position_ = root_->children_.end();
        Node* parent = root_.get();
        auto iter = root_->children_.begin();
        while (!parent->is_root())
        {
            if (iter != parent->children_.end())
            {
                auto& node = iter->second;
                node.position_ = iter;
                node.parent_ = parent;

                if (!node.is_leaf())
                {
                    auto& child = iter->second;
                    parent = &child;
                    iter = child.children_.begin();
                }
                else
                {
                    ++iter;
                }
            }
            else
            {
                iter = parent->position_;
                parent = parent->parent_;
                if (!parent->is_root())
                    ++iter;
            }
        }
    }

    template <typename... Us>
    std::pair<Cursor, bool> upsert(bool clobber, Key key, Us&&... args)
    {
        // To avoid dangling link nodes in the event of an exception,
        // build a sub-chain first with the new node, than attach it to the
        // existing tree using move semantics.

        assert(!key.empty());
        assert(root_ != nullptr);

        const auto tokenCount = key.size();
        auto parent = root_.get();
        auto child = root_->children_.begin();

        // Find existing node from which to possibly attach a sub-chain with
        // the new node.
        Size level = 0;
        for (; level < tokenCount; ++level)
        {
            const auto& token = key[level];
            child = parent->children_.find(token);
            if (child == parent->children_.end())
                break;
            parent = &(child->second);
        }

        // Check if node already exists at the destination level
        // in the existing tree.
        if (level == tokenCount)
        {
            bool placed = false;
            auto& node = child->second;
            parent = node.parent_;
            placed = !node.value().has_value();
            if (placed || clobber)
                node.value_.emplace(std::forward<Us>(args)...);
            return {{*parent, child}, placed};
        }

        // Check if only a single value node needs to be added
        assert(level < tokenCount);
        if (tokenCount - level == 1)
        {
            child = addValueNode(parent, key[level], std::forward<Us>(args)...);
            child->second.position_ = child;
            child->second.parent_ = parent;
            return {{*parent, child}, true};
        }

        // Build and attach the sub-chain containing the new node.
        Node chain;
        auto token = std::move(key[level]);
        buildChain(&chain, std::move(key), level, std::forward<Us>(args)...);
        child = addChain(parent, std::move(token), std::move(chain));
        parent = child->second.parent_;
        return {{*parent, child}, true};
    }

    template <typename... Us>
    static TreeIterator addValueNode(Node* node, Token label, Us&&... args)
    {
        auto result = node->children_.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(std::move(label)),
            std::forward_as_tuple(in_place, std::forward<Us>(args)...));
        assert(result.second);
        return result.first;
    }

    template <typename... Us>
    static void buildChain(Node* node, Key&& key, Size level, Us&&... args)
    {
        const auto tokenCount = key.size();
        ++level;

        // Add intermediary link nodes
        for (; level < tokenCount - 1; ++level)
        {
            auto iter = buildLink(*node, std::move(key[level]));
            node = &(iter->second);
        }

        // Add value node
        assert(level < key.size());
        addValueNode(node, std::move(key[level]), std::forward<Us>(args)...);
    }

    template <typename... Us>
    static TreeIterator buildLink(Node& node, Token label)
    {
        auto result = node.children_.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(std::move(label)),
            std::forward_as_tuple());
        assert(result.second);
        return result.first;
    }

    static TreeIterator addChain(Node* parent, Token&& label, Node&& chain)
    {
        auto result = parent->children_.emplace(std::move(label),
                                                std::move(chain));
        assert(result.second);

        // Traverse down the emplaced chain and set the parent/position
        // fields to their proper values. Better to do this after emplacing
        // the chain to avoid invalid pointers/iterators.
        auto iter = result.first;
        while (!parent->is_leaf())
        {
            Node& child = iter->second;
            child.position_ = iter;
            child.parent_ = parent;
            parent = &child;
            iter = child.children_.begin();
        }
        return parent->position_;
    }

    Node sentinel_;
    std::unique_ptr<Node> root_;
    Size size_ = 0;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_TOKENTRIEIMPL_HPP
