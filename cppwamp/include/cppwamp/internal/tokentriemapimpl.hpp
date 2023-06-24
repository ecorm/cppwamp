/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_TOKENTRIEMAPIMPL_HPP
#define CPPWAMP_INTERNAL_TOKENTRIEMAPIMPL_HPP

#include <algorithm>
#include <cassert>
#include <memory>
#include <tuple>
#include <utility>
#include "../utils/tokentriemapnode.hpp"

namespace wamp
{

namespace utils
{

namespace internal
{

//------------------------------------------------------------------------------
template <typename K, typename T, typename C, typename A>
class TokenTrieMapImpl
{
public:
    using Key = K;
    using Value = T;
    using KeyComp = C;
    using Allocator = A;
    using AllocTraits = std::allocator_traits<A>;
    using Node = TokenTrieMapNode<K, T, C, A>;
    using Size = typename Node::tree_type::size_type;
    using Cursor = TokenTrieMapCursor<Node, true>;
    using ConstCursor = TokenTrieMapCursor<Node, false>;

    class ValueComp
    {
    public:
        using result_type = bool;
        using first_argument_type = Value;
        using second_argument_type = Value;

        ValueComp() = default;

        bool operator()(const Value& a, const Value& b)
        {
            return comp(a.first, b.first);
        }

    protected:
        // NOLINTNEXTLINE(google-explicit-constructor)
        ValueComp(KeyComp c) : comp(std::move(c)) {}

        // NOLINTBEGIN(cppcoreguidelines-non-private-member-variables-in-classes)
        // NOLINTBEGIN(misc-non-private-member-variables-in-classes)
        KeyComp comp;
        // NOLINTEND(misc-non-private-member-variables-in-classes)
        // NOLINTEND(cppcoreguidelines-non-private-member-variables-in-classes)

        friend class TokenTrieMapImpl;
    };

    explicit TokenTrieMapImpl(const KeyComp& comp, const Allocator& alloc)
        : sentinel_(PassKey{}),
          alloc_(alloc),
          comp_(comp)
    {}

    TokenTrieMapImpl(const TokenTrieMapImpl& rhs)
        : TokenTrieMapImpl(rhs,
                           AllocTraits::select_on_container_copy_construction(
                               rhs.alloc_))
    {}

    TokenTrieMapImpl(const TokenTrieMapImpl& rhs, const Allocator& alloc)
        : sentinel_(PassKey{}),
          alloc_(alloc),
          size_(rhs.size_),
          comp_(rhs.comp_)
    {
        if (rhs.root_)
        {
            root_ = copyConstructNode(*rhs.root_);
            auto& root = *root_;
            root.parent_ = &sentinel_;
            scanTree();
        }
    }

    TokenTrieMapImpl(TokenTrieMapImpl&& rhs) noexcept
        : TokenTrieMapImpl(std::move(rhs), rhs.alloc_)
    {}

    TokenTrieMapImpl(TokenTrieMapImpl&& rhs, const Allocator& alloc) noexcept
        : sentinel_(PassKey{}),
          alloc_(alloc),
          size_(rhs.size_),
          comp_(std::move(rhs.comp_))
    {
        moveRootFrom(rhs);
    }

    ~TokenTrieMapImpl()
    {
        if (root_)
        {
            destroyNode(root_);
            root_ = {};
        }
    }

    TokenTrieMapImpl& operator=(const TokenTrieMapImpl& rhs)
    {
        // Do nothing for self-assignment to enfore the invariant that
        // the RHS iterators remain valid.
        if (&rhs != this)
        {
            copyAssign(
                typename AllocTraits::propagate_on_container_copy_assignment{},
                rhs);
        }
        return *this;
    }

    TokenTrieMapImpl& operator=(TokenTrieMapImpl&& rhs) noexcept
    {
        // Do nothing for self-move-assignment to avoid invalidating iterators.
        if (&rhs != this)
        {
            moveAssign(
                typename AllocTraits::propagate_on_container_move_assignment{},
                rhs);
        }
        return *this;
    }

    KeyComp keyComp() const {return comp_.comp;}

    ValueComp valueComp() const {return comp_;}

    Allocator allocator() const {return alloc_;}

    Cursor rootCursor()
    {
        if (empty())
            return sentinelCursor();
        return Cursor::begin(*root_);
    }

    ConstCursor rootCursor() const
    {
        if (empty())
            return sentinelCursor();
        return ConstCursor::begin(*root_);
    }

    Cursor firstValueCursor()
    {
        if (empty())
            return sentinelCursor();
        return Cursor::first(*root_);
    }

    ConstCursor firstValueCursor() const
    {
        if (empty())
            return sentinelCursor();
        return ConstCursor::first(*root_);
    }

    Cursor sentinelCursor() {return Cursor::end(sentinel_);}

    ConstCursor sentinelCursor() const {return ConstCursor::end(sentinel_);}

    Cursor locate(const Key& key)
    {
        return locateElement<Cursor>(*this, key);
    }

    ConstCursor locate(const Key& key) const
    {
        return locateElement<ConstCursor>(*this, key);
    }

    Cursor lowerBound(const Key& key)
    {
        return findLowerBound<Cursor>(*this, key);
    }

    ConstCursor lowerBound(const Key& key) const
    {
        return findLowerBound<ConstCursor>(*this, key);
    }

    Cursor upperBound(const Key& key)
    {
        return findUpperBound<Cursor>(*this, key);
    }

    ConstCursor upperBound(const Key& key) const
    {
        return findUpperBound<ConstCursor>(*this, key);
    }

    std::pair<Cursor, Cursor> equalRange(const Key& key)
    {
        return {lowerBound(key), upperBound(key)};
    }

    std::pair<ConstCursor, ConstCursor> equalRange(const Key& key) const
    {
        return {lowerBound(key), upperBound(key)};
    }

    Size empty() const {return size_ == 0;}

    Size size() const {return size_;}

    void clear()
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
            root_ = constructNode();
            auto& root = *root_;
            root.parent_ = &sentinel_;
            root.position_ = root.children_.end();
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
        pos.advance_depth_first_to_next_element();

        cursor.target_->second.clearValue();
        if (cursor.target()->is_leaf())
        {
            // Erase the value node, then all obsolete links up the chain
            // until we hit another value node or the sentinel.
            while (!cursor.has_element() && !cursor.at_end())
            {
                cursor.parent_->children_.erase(cursor.target_);
                cursor.target_ = cursor.parent_->position_;
                cursor.parent_ = cursor.parent_->parent_;
            }
        }

        --size_;
        return pos;
    }

    void swap(TokenTrieMapImpl& other)
    {
        doSwap(typename AllocTraits::propagate_on_container_swap{}, other);
    }

    template <typename TOther>
    bool equals(const TOther& rhs) const
    {
        if (empty() || rhs.empty())
            return empty() == rhs.empty();

        auto curA = rootCursor();
        auto curB = rhs.rootCursor();
        while (!curA.at_end())
        {
            if (curB.at_end())
                return false;
            if (curA.token_or_value_differs(curB))
                return false;
            curA.advance_depth_first_to_next_node();
            curB.advance_depth_first_to_next_node();
        }
        return curB.at_end();
    }

    template <typename TOther>
    bool differs(const TOther& rhs) const
    {
        if (empty() || rhs.empty())
            return empty() != rhs.empty();

        auto curA = rootCursor();
        auto curB = rhs.rootCursor();
        while (!curA.at_end())
        {
            if (curB.at_end())
                return true;
            if (curA.token_or_value_differs(curB))
                return true;
            curA.advance_depth_first_to_next_node();
            curB.advance_depth_first_to_next_node();
        }
        return !curB.at_end();
    }

private:
    using Tree = typename Node::tree_type;
    using TreeIterator = typename Tree::iterator;
    using TreeAllocator = typename Node::tree_allocator_type;
    using Token = typename Node::token_type;
    using Level = typename Key::size_type;
    using NodeAllocator = typename AllocTraits::template rebind_alloc<Node>;
    using NodeAllocTraits = typename AllocTraits::template rebind_traits<Node>;
    using NodePtr = typename NodeAllocTraits::pointer;
    using PassKey = typename Node::PassKey;

    template <typename TSelf>
    void copyAssign(TrueType, const TSelf& rhs)
    {
        TokenTrieMapImpl temp(rhs, rhs.alloc_);
        moveAssign(TrueType{}, temp);
    }

    template <typename TSelf>
    void copyAssign(FalseType, const TSelf& rhs)
    {
        TokenTrieMapImpl temp(rhs, alloc_);
        moveAssign(FalseType{}, temp);
    }

    template <typename TSelf>
    void moveAssign(TrueType, TSelf& rhs)
    {
        size_ = rhs.size_;
        comp_ = std::move(rhs.comp_);
        moveRootFrom(rhs);
        alloc_ = std::move(rhs.alloc_);
    }

    template <typename TSelf>
    void moveAssign(FalseType, TSelf& rhs)
    {
        comp_ = std::move(rhs.comp_);
        if (alloc_ == rhs.alloc_)
        {
            size_ = rhs.size_;
            moveRootFrom(rhs);
            alloc_ = std::move(rhs.alloc_);
        }
        else
        {
            clear();
            auto cursor = rhs.firstValueCursor();
            auto sentinel = rhs.sentinelCursor();
            while (cursor != sentinel)
            {
                put(false, cursor.key(), cursor.value());
                cursor.advance_depth_first_to_next_element();
            }
        }
    }

    void moveRootFrom(TokenTrieMapImpl& rhs)
    {
        if (root_)
            destroyNode(root_);
        root_ = rhs.root_;
        if (root_)
            (*root_).parent_ = &sentinel_;
        rhs.root_ = {};
        rhs.size_ = 0;
    }

    template <typename TSelf>
    void doSwap(TrueType, TSelf& other)
    {
        using std::swap;
        swap(alloc_, other.alloc_);
        swap(root_, other.root_);
        swap(size_, other.size_);
        swap(comp_, other.comp_);
        if (root_)
            (*root_).parent_ = &sentinel_;
        if (other.root_)
            (*other.root_).parent_ = &other.sentinel_;
    }

    template <typename TSelf>
    void doSwap(FalseType, TSelf& other)
    {
        if (alloc_ == other.alloc_)
        {
            using std::swap;
            swap(root_, other.root_);
            swap(size_, other.size_);
            swap(comp_, other.comp_);
            if (root_)
            {
                auto& root = *root_;
                root.parent_ = &sentinel_;
            }
            if (other.root_)
            {
                auto& root = *(other.root_);
                root.parent_ = &other.sentinel_;
            }
        }
        else
        {
            TokenTrieMapImpl thisTemp(*this, other.alloc_);
            TokenTrieMapImpl otherTemp(other, alloc_);
            moveAssign(TrueType{}, otherTemp);
            other.moveAssign(TrueType{}, thisTemp);
        }
    }

    NodePtr constructNode()
    {
        NodePtr ptr = {};
        try
        {
            ptr = NodeAllocTraits::allocate(alloc_, sizeof(Node));
            NodeAllocTraits::construct(alloc_, ptr, PassKey{}, keyComp());
        }
        catch (...)
        {
            if (!!ptr)
                NodeAllocTraits::deallocate(alloc_, ptr, sizeof(Node));
            throw;
        }

        return ptr;
    }

    NodePtr copyConstructNode(const Node& rhs)
    {
        auto ptr = NodeAllocTraits::allocate(alloc_, sizeof(Node));
        NodeAllocTraits::construct(alloc_, ptr, rhs);
        return ptr;
    }

    void destroyNode(NodePtr node)
    {
        NodeAllocTraits::destroy(alloc_, node);
        NodeAllocTraits::deallocate(alloc_, node, sizeof(Node));
    }

    void scanTree()
    {
        auto& root = *root_;
        root.position_ = root.children_.end();
        Node* parent = &root;
        auto iter = root.children_.begin();
        while (!parent->is_sentinel())
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
                if (!parent->is_sentinel())
                    ++iter;
            }
        }
    }

    template <typename TCursor, typename TSelf>
    static TCursor locateElement(TSelf& self, const Key& key)
    {
        if (self.empty() || key.empty())
            return self.sentinelCursor();

        auto& root = *(self.root_);
        Node* parent = &root;
        auto child = root.children_.begin();
        bool found = true;
        for (Level level = 0; level<key.size(); ++level)
        {
            const auto& token = key[level];
            child = parent->children_.find(token);
            if (child == parent->children().end())
            {
                found = false;
                break;
            }

            if (level < key.size() - 1)
                parent = &(child->second);
        }

        if (!found || !child->second.has_element())
            return self.sentinelCursor();
        return {parent, child};
    }

    template <typename... Us>
    std::pair<Cursor, bool> upsert(bool clobber, Key key, Us&&... args)
    {
        // To avoid dangling link nodes in the event of an exception,
        // build a sub-chain first with the new node, than attach it to the
        // existing tree using move semantics.

        assert(!key.empty());
        assert(!!root_);

        const auto tokenCount = key.size();
        auto& root = *root_;
        Node* parent = &root;
        auto child = root.children_.begin();

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
            placed = !node.has_element();
            if (placed || clobber)
                node.setElement(std::move(key), std::forward<Us>(args)...);
            return {{parent, child}, placed};
        }

        // Check if only a single value node needs to be added
        assert(level < tokenCount);
        if (tokenCount - level == 1)
        {
            auto token = key[level];
            child = addValueNode(parent, std::move(token), std::move(key),
                                 std::forward<Us>(args)...);
            child->second.position_ = child;
            child->second.parent_ = parent;
            return {{parent, child}, true};
        }

        // Build and attach the sub-chain containing the new node.
        auto chain = constructNode();
        auto token = key[level];
        buildChain(chain, std::move(key), level, std::forward<Us>(args)...);
        try
        {
            child = addChain(parent, std::move(token), std::move(*chain));
        }
        catch (...)
        {
            destroyNode(chain);
            throw;
        }
        parent = child->second.parent_;
        destroyNode(chain);
        return {{parent, child}, true};
    }

    template <typename... Us>
    TreeIterator addValueNode(Node* node, Token&& label, Key&& key,
                              Us&&... args)
    {
        auto result = node->children_.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(std::move(label)),
            std::forward_as_tuple(PassKey{}, keyComp(), std::move(key),
                                  in_place, std::forward<Us>(args)...));
        assert(result.second);
        return result.first;
    }

    template <typename... Us>
    void buildChain(Node* node, Key&& key, Size level, Us&&... args)
    {
        const auto tokenCount = key.size();
        ++level;

        // Add intermediary link nodes
        for (; level < tokenCount - 1; ++level)
        {
            auto iter = addLink(*node, key[level]);
            node = &(iter->second);
        }

        // Add value node
        assert(level < key.size());
        auto token = key[level];
        addValueNode(node, std::move(token), std::move(key),
                     std::forward<Us>(args)...);
    }

    template <typename... Us>
    TreeIterator addLink(Node& node, const Token& label)
    {
        auto result = node.children_.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(label),
            std::forward_as_tuple(PassKey{}, keyComp()));
        assert(result.second);
        return result.first;
    }

    static TreeIterator addChain(Node* parent, Token&& label, Node&& chain)
    {
        auto result = parent->children_.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(std::move(label)),
            std::forward_as_tuple(PassKey{}, std::move(chain)));
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

    template <typename TCursor, typename TSelf>
    static TCursor findLowerBound(TSelf& self, const Key& key)
    {
        if (key.empty() || self.empty())
            return self.sentinelCursor();

        auto& root = *(self.root_);
        Node* parent = &root;
        auto child = root.children_.begin();
        const Level maxLevel = key.size() - 1;
        KeyComp keyComp;
        bool keepSearching = false;

        for (Level level = 0; level <= maxLevel; ++level)
        {
            const auto& token = key[level];
            child = parent->children_.lower_bound(token);
            if (child == parent->children_.end())
                break;

            bool isNotEquivalent = keyComp(token, child->first);
            if (isNotEquivalent)
                break;

            if (level < maxLevel)
            {
                if (child->second.is_leaf() )
                {
                    keepSearching = true;
                    break;
                }
                parent = &(child->second);
                child = parent->children_.begin();
            }
        }

        TCursor cursor{parent, child};

        while (keepSearching)
        {
            cursor.advance_depth_first_to_next_element();
            keepSearching = !cursor.at_end() && keyComp(cursor.key(), key);
        }

        if (!cursor.has_element())
            cursor.advance_depth_first_to_next_element();

        return cursor;
    }

    template <typename TCursor, typename TSelf>
    static TCursor findUpperBound(TSelf& self, const Key& key)
    {
        if (key.empty() || self.empty())
            return self.sentinelCursor();

        auto& root = *(self.root_);
        auto parent = &root;
        auto child = root.children_.begin();
        const Level maxLevel = key.size() - 1;
        KeyComp keyComp;
        bool keepSearching = false;

        for (Level level = 0; level <= maxLevel; ++level)
        {
            const auto& token = key[level];
            child = parent->children_.lower_bound(token);

            if (child == parent->children_.end())
                break;

            bool isNotEquivalent = keyComp(token, child->first);
            if (isNotEquivalent)
                break;

            if (child->second.is_leaf())
            {
                child = parent->children_.upper_bound(token);
                break;
            }

            if (level < maxLevel)
            {
                parent = &(child->second);
                child = parent->children_.begin();
            }
            else
            {
                keepSearching = true;
            }
        }

        TCursor cursor{parent, child};

        while (keepSearching)
        {
            cursor.advance_depth_first_to_next_element();
            keepSearching = !cursor.at_end() && !keyComp(key, cursor.key());
        }

        if (!cursor.has_element())
            cursor.advance_depth_first_to_next_element();

        return cursor;
    }

    Node sentinel_;
    NodeAllocator alloc_;
    NodePtr root_ = nullptr;
    Size size_ = 0;
    ValueComp comp_;
};

} // namespace internal

} // namespace utils

} // namespace wamp

#endif // CPPWAMP_INTERNAL_TOKENTRIEMAPIMPL_HPP
