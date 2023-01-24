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
    using key_type = K;

    using mapped_type = T;

    using policy_type = P;

    using value_storage = typename P::value_storage;

    using Node = TokenTrieNode<key_type, value_storage>;

    using size_type = typename Node::tree_type::size_type;

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

    Cursor firstTerminalCursor()
    {
        if (empty())
            return sentinelCursor();
        auto cursor = rootCursor();
        cursor.advanceToFirstTerminal();
        return cursor;
    }

    Cursor firstTerminalCursor() const
    {
        return const_cast<TokenTrieImpl&>(*this).firstTerminalCursor();
    }

    Cursor sentinelCursor()
    {
        return Cursor::end(sentinel_);
    }

    Cursor sentinelCursor() const
    {
        return Cursor::end(const_cast<Node&>(sentinel_));
    }

    Cursor locate(const key_type& key)
    {
        if (empty() || key.empty())
            return sentinelCursor();
        auto cursor = rootCursor();
        cursor.locate(key);
        return cursor;
    }

    Cursor locate(const key_type& key) const
    {
        return const_cast<TokenTrieImpl&>(*this).locate(key);
    }

    Cursor lowerBound(const key_type& key) const
    {
        if (empty() || key.empty())
            return sentinelCursor();
        auto cursor = rootCursor();
        cursor.lower_bound(key);
        return cursor;
    }

    Cursor upperBound(const key_type& key) const
    {
        if (empty() || key.empty())
            return sentinelCursor();
        auto cursor = rootCursor();
        cursor.upper_bound(key);
        return cursor;
    }

    std::pair<Cursor, Cursor> equalRange(const key_type& key) const
    {
        if (empty() || key.empty())
            return {sentinelCursor(), sentinelCursor()};
        return Cursor::equal_range(*root_, key);
    }

    size_type empty() const noexcept {return size_ == 0;}

    size_type size() const noexcept {return size_;}

    void clear() noexcept
    {
        if (root_)
            root_->children_.clear();
        size_ = 0;
    }

    template <typename... Us>
    std::pair<Cursor, bool> put(bool clobber, key_type key, Us&&... args)
    {
        if (key.empty())
            return {sentinelCursor(), false};

        if (!root_)
        {
            root_.reset(new Node);
            root_->parent_ = &sentinel_;
        }

        auto cursor = rootCursor();
        bool placed = bool(cursor.put(clobber, std::move(key),
                                      std::forward<Us>(args)...));
        if (placed)
            ++size_;
        return {cursor, placed};
    }

    Cursor erase(Cursor pos)
    {
        auto cursor = pos;
        assert(bool(cursor));
        pos.advance_to_next_terminal();
        cursor.eraseFromHere();
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

    Node sentinel_;
    std::unique_ptr<Node> root_;
    size_type size_ = 0;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_TOKENTRIEIMPL_HPP
