/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_UTILS_TOKENTRIENODE_HPP
#define CPPWAMP_UTILS_TOKENTRIENODE_HPP

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

#include "../tagtypes.hpp"
#include "treeview.hpp"

namespace wamp
{

namespace utils
{

namespace internal
{
    template <typename, typename, typename, typename> class TokenTrieImpl;
}

//------------------------------------------------------------------------------
// TODO: Store pair<const K, T> in the node to avoid proxy mess.
template <typename K, typename T, typename C, typename A>
class TokenTrieNode
{
private:
    struct PassKey {};

public:
    /** Split token container type used as the key. */
    using key_type = K;

    /** Type of the mapped value. */
    using mapped_type = T;

    /// Comparison function that determines how keys are sorted.
    using key_compare = C;

    /// Allocator type passed to the TokenTrie using this node.
    using allocator_type = A;

    /// Token type associated with a node.
    using token_type = typename key_type::value_type;

    /// Allocator type used by the tree contained by this node.
    using tree_allocator_type =
        typename std::allocator_traits<A>::template rebind_alloc<
            std::pair<const token_type, TokenTrieNode>>;

    /// Tree type contained by this node.
    using tree_type = std::map<token_type, TokenTrieNode, key_compare,
                               tree_allocator_type>;

    /** Wrapper around the contained tree that prevents modifying its structure,
        while allowing its mapped values to be modified. */
    using tree_view_type = TreeView<tree_type, true>;

    /** Wrapper around the contained tree that prevents modifying its structure
        and its mapped values. */
    using const_tree_view_type = TreeView<tree_type, false>;

    /** Copy constructor. */
    TokenTrieNode(const TokenTrieNode& other)
        : children_(other.children_)
    {
        if (other.has_value())
            constructValue(other.value());
    }

    /** Copy constructor taking an allocator. */
    TokenTrieNode(std::allocator_arg_t, const tree_allocator_type& alloc,
                  const TokenTrieNode& other)
        : children_(other.children_, alloc)
    {
        if (other.has_value())
            constructValue(other.value());
    }

    /** Non-move-constructible. */
    TokenTrieNode(TokenTrieNode&& other) = delete;

    /** Non-copy-assignable. */
    TokenTrieNode& operator=(const TokenTrieNode&) = delete;

    /** Non-move-assignable. */
    TokenTrieNode& operator=(TokenTrieNode&&) = delete;

    /** Destructor. */
    ~TokenTrieNode() {destroyValue();}

    /** Determines if this is the sentinel node. */
    bool is_sentinel() const noexcept {return parent_ == nullptr;}

    /** Determines if this is the root node. */
    bool is_root() const noexcept
        {return !is_sentinel() && parent_->is_sentinel();}

    /** Returns true is this node has no children. */
    bool is_leaf() const noexcept {return children_.empty();}

    /** Determines if this node has a mapped value. */
    bool has_value() const noexcept {return !!value_;}

    /** Obtains a pointer to the node's parent, or `nullptr` if this is
        the sentinel node. */
    TokenTrieNode* parent() noexcept {return parent_;}

    /** Obtains a pointer to the node's parent, or `nullptr` if this is
        the sentinel node. */
    const TokenTrieNode* parent() const noexcept {return parent_;}

    /** Accesses the node's token, or an empty one if this is the root node.
        @pre `!this->is_sentinel` */
    const token_type& token() const
    {
        assert(!is_sentinel());
        static token_type emptyToken;
        if (is_root())
            return emptyToken;
        return position_->first;
    }

    /** Generates the split token key associated with this node.
        @pre `!this->is_sentinel` */
    key_type key() const
    {
        assert(!is_sentinel());
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

    /** Accesses the value associated with this node.
        @pre `this->has_value()` */
    mapped_type& value() {assert(has_value()); return *value_;}

    /** Accesses the value associated with this node.
        @pre `this->has_value()` */
    const mapped_type& value() const {assert(has_value()); return *value_;}

    /** Obtains a view of the node's child tree.
        @pre `!this->is_sentinel` */
    tree_view_type children() {assert(!is_sentinel()); return {&children_};}

    /** Obtains a view of the node's child tree.
        @pre `!this->is_sentinel` */
    const_tree_view_type children() const
        {assert(!is_sentinel()); return {&children_};}

private:
    using TreeIterator = typename tree_type::iterator;
    using MappedAlloc =
        typename std::allocator_traits<A>::template rebind_alloc<T>;
    using MappedPtr = typename std::allocator_traits<MappedAlloc>::pointer;

    template <typename... Us>
    void setValue(Us&&... args)
    {
        if (has_value())
            *value_ = mapped_type(std::forward<Us>(args)...);
        else
            constructValue(std::forward<Us>(args)...);
    }

    void clearValue()
    {
        if (has_value())
            destroyValue();
    }

    template <typename... Us>
    void constructValue(Us&&... args)
    {
        using AT = std::allocator_traits<MappedAlloc>;
        assert(value_ == nullptr);
        MappedAlloc alloc{children_.get_allocator()};
        MappedPtr ptr = {};
        try
        {
            ptr = AT::allocate(alloc, sizeof(mapped_type));
            AT::construct(alloc, ptr, std::forward<Us>(args)...);
        }
        catch (...)
        {
            if (!!ptr)
                AT::deallocate(alloc, ptr, sizeof(mapped_type));
            throw;
        }
        value_ = ptr;
    }

    void destroyValue()
    {
        if (!has_value())
            return;

        using AT = std::allocator_traits<MappedAlloc>;
        MappedAlloc alloc{children_.get_allocator()};
        AT::destroy(alloc, value_);
        AT::deallocate(alloc, value_, sizeof(mapped_type));
        value_ = {};
    }

    tree_type children_;
    TreeIterator position_ = {};
    MappedPtr value_ = {};
    TokenTrieNode* parent_ = nullptr;

    template <typename, bool> friend class TokenTrieCursor;

    template <typename, typename, typename, typename>
    friend class internal::TokenTrieImpl;

public: // Internal use only
    TokenTrieNode(PassKey) : position_(children_.end()) {}

    TokenTrieNode(PassKey, key_compare comp)
        : children_(std::move(comp)),
          position_(children_.end())
    {}

    TokenTrieNode(std::allocator_arg_t, const tree_allocator_type& alloc,
                  PassKey, key_compare comp)
        : children_(std::move(comp), alloc),
          position_(children_.end())
    {}

    template <typename... Us>
    TokenTrieNode(PassKey, key_compare comp, in_place_t, Us&&... args)
        : children_(std::move(comp))
    {
        constructValue(std::forward<Us>(args)...);
    }

    template <typename... Us>
    TokenTrieNode(std::allocator_arg_t, const tree_allocator_type& alloc,
                  PassKey, key_compare comp, in_place_t, Us&&... args)
        : children_(std::move(comp), alloc)
    {
        constructValue(std::forward<Us>(args)...);
    }

    TokenTrieNode(PassKey, TokenTrieNode&& other)
        : children_(std::move(other.children_)),
          position_(std::move(other.position_)),
          value_(other.value_),
          parent_(other.parent_)
    {
        other.value_ = nullptr;
    }

    TokenTrieNode(std::allocator_arg_t, const tree_allocator_type& alloc,
                  PassKey, TokenTrieNode&& other)
        : children_(std::move(other.children_)),
          position_(std::move(other.position_)),
          value_(other.value_),
          parent_(other.parent_)
    {
        other.value_ = nullptr;
    }
};

//------------------------------------------------------------------------------
/** Type used to traverse nodes in a TokenTrie.
    This type intended for trie algorithms where a forward iterator that only
    traverses value nodes is insuffient.
    @tparam N The node type being traversed.
    @tparam IsMutable Allows node values to be modified when true. */
//------------------------------------------------------------------------------
template <typename N, bool IsMutable>
class TokenTrieCursor
{
public:
    /// Node type being traversed.
    using node_type = N;

    /// Split token container type used as the key.
    using key_type = typename N::key_type;

    /// Comparison function that determines how keys are sorted.
    using key_compare = typename N::key_compare;

    /// Type of the token associated with a node.
    using token_type = typename N::token_type;

    /// Integral type used to represent the depth within the tree.
    using level_type = typename key_type::size_type;

    /// Type of the mapped value.
    using mapped_type = typename N::mapped_type;

    /** Wrapper around a node tree that prevents modifying its structure. */
    using tree_view_type = TreeView<typename N::tree_type, IsMutable>;

    /** Wrapper around a node tree that prevents modifying its structure. */
    using const_tree_view_type = TreeView<typename N::tree_type, false>;

    /** Reference to a mapped value. */
    using reference = typename std::conditional<IsMutable, mapped_type&,
                                                const mapped_type&>::type;

    /** Iterator type which advances through a tree's child nodes in a
        breath-first manner (non-recursive). */
    using const_iterator = typename node_type::tree_type::const_iterator;

    /** Iterator type which advances through a tree's child nodes in a
        breath-first manner (non-recursive). */
    using iterator =
        typename std::conditional<
            IsMutable,
            typename node_type::tree_type::iterator,
            const_iterator>::type;
    using node_pointer = typename std::conditional<IsMutable, node_type*,
                                                   const node_type*>::type;

    /** True if this cursor allows mapped values to be modified. */
    static constexpr bool is_mutable() {return IsMutable;}

    /** Default constructs a cursor that does not point to any node. */
    TokenTrieCursor() = default;

    /** Conversion from mutable cursor to const cursor. */
    template <bool RM, typename std::enable_if<!IsMutable && RM, int>::type = 0>
    TokenTrieCursor(const TokenTrieCursor<N, RM>& rhs)
        : parent_(rhs.parent_),
          target_(rhs.target_)
    {}

    /** Assignment from mutable cursor to const cursor. */
    template <bool RM, typename std::enable_if<!IsMutable && RM, int>::type = 0>
    TokenTrieCursor& operator=(const TokenTrieCursor<N, RM>& rhs)
    {
        parent_ = rhs.parent_;
        target_ = rhs.target_;
        return *this;
    }

    /** Same as TokenTrieCursor::good */
    explicit operator bool() const noexcept {return good();}

    /** Returns true if the cursor points to a valid node (which may or may
        not contain a value). */
    bool good() const noexcept {return !at_end() && !at_end_of_level();}

    /** Determines if the cursor reached the end of the entire trie. */
    bool at_end() const noexcept {return !parent_ || !parent_->parent();}

    /** Determines if the cursor reached the end of a level, or the end of
        the entire trie. */
    bool at_end_of_level() const noexcept
        {return at_end() || target_ == parent_->children().end();}

    /** Determines if the cursor points to a node containing a mapped value. */
    bool has_value() const noexcept
        {return !at_end_of_level() && childNode().has_value();}

    /** Determines if the token and mapped value of this cursor's node are
        equivalent to the ones from the given cursor.
        If either cursor is not good, they are considered equivalent if and
        only if both cursors are not good. */
    bool token_and_value_equals(const TokenTrieCursor& rhs) const
    {
        if (!good())
            return !rhs.good();
        if (!rhs.good() || tokensAreNotEquivalent(token(), rhs.token()))
            return false;

        const auto& a = childNode();
        const auto& b = rhs.childNode();
        return a.has_value() ? (b.has_value() && (a.value() == b.value()))
                             : !b.has_value();
    }

    /** Determines if the token or mapped value of this cursor's node are
        different to the ones from the given cursor.
        If either cursor is not good, they are considered different if and
        only if the cursors are not both bad. */
    bool token_or_value_differs(const TokenTrieCursor& rhs) const
    {
        if (!good())
            return rhs.good();
        if (!rhs.good() || tokensAreNotEquivalent(token(), rhs.token()))
            return true;

        const auto& a = childNode();
        const auto& b = rhs.childNode();
        return a.has_value() ? (!b.has_value() || (a.value() != b.value()))
                             : b.has_value();
    }

    /** Returns a pointer to the target node's parent, or `nullptr` if
        the target is the sentinel node. */
    node_pointer parent() noexcept {return parent_;}

    /** Returns a pointer to the target node's parent, or `nullptr` if
        the target is the sentinel node. */
    const node_type* parent() const noexcept {return parent_;}

    /** Returns a pointer to the target node, or `nullptr` if the cursor
        is not good(). */
    node_pointer target() noexcept
        {return good() ? &(target_->second) : nullptr;}

    /** Returns a pointer to the target node, or `nullptr` if the cursor
        is not good(). */
    const node_type* target() const noexcept
         {return good() ? &(target_->second) : nullptr;}

    /** Obtains a view of the parent's child tree.
        @pre `!this->at_end()` */
    tree_view_type children() {return parentNode().children();}

    /** Obtains a view of the parent's child tree.
        @pre `!this->at_end()` */
    const_tree_view_type children() const {return parentNode().children();}

    /** Obtains an iterator to the target node's position within the parent's
        child tree.
        @pre `!this->at_end()` */
    iterator iter() {assert(!at_end()); return target_;}

    /** Obtains an iterator to the target node's position within the parent's
        child tree.
        @pre `!this->at_end()` */
    const_iterator iter() const {assert(!at_end()); return target_;}

    /** Generates the key associated with the current target node.
        @pre `!this->at_end_of_level() */
    key_type key() const {return childNode().key();}

    /** Obtains the token associated with the current target node.
        @pre `!this->at_end_of_level() */
    const token_type& token() const
        {assert(!at_end_of_level()); return target_->first;}

    /** Accesses the mapped value associated with the current target node.
        @pre `this->has_value()` */
    const mapped_type& value() const
        {assert(has_value()); return target_->second.value();}

    /** Accesses the mapped value associated with the current target node.
        @pre `this->has_value()` */
    reference value()
        {assert(has_value()); return target_->second.value();}

    /** Makes the cursor advance in a depth-first manner to point the next node
        in the trie. Does not advance if already at the sentinel node. */
    void advance_depth_first_to_next_node() noexcept
    {
        while (!at_end())
        {
            advanceDepthFirst();
            if (target_ != parent_->children_.end())
                break;
        }
    }

    /** Makes the cursor advance in a depth-first manner to point the next node
        in the trie having a mapped value. Does not advance if already at the
        sentinel node. */
    void advance_depth_first_to_next_element() noexcept
    {
        while (!at_end())
        {
            advanceDepthFirst();
            if (has_value())
                break;
        }
    }

    /** Makes the cursor advance in a breadth-first manner to point the next
        node within the same level in the trie. Does not advance is already
        at the end of the level. */
    void advance_to_next_node_in_level() noexcept
    {
        if (!at_end_of_level())
            ++target_;
    }

    /** Makes the cursor point to the node within the same level that is
        associated with the given iterator. The given iterator may be the end
        iterator of the current level. */
    void skip_to(iterator iter) noexcept {target_ = iter;}

    /** Makes the cursor point to the current target node's parent. Does not
        ascend if already at the root.
        @returns `level-1` if ascension occurred, `level` otherwise.
        @pre `level > 0 || this->parent()->is_sentinel()` */
    level_type ascend(
        level_type level ///< Index of the current level.
        )
    {
        target_ = parent_->position_;
        parent_ = parent_->parent_;
        if (!parent_->is_sentinel())
        {
            assert(level > 0);
            --level;
        }
        return level;
    }

    /** Makes the cursor point to the first child of the current target node.
        @returns `level+1`
        @pre `this->good()` */
    level_type descend(
        level_type level ///< Index of the current level.
        )
    {
        assert(good());
        auto& child = target_->second;
        assert(!child.is_leaf());
        parent_ = &child;
        target_ = child.children_.begin();
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
        if (!cursor.at_end_of_level() && !cursor.target()->has_value())
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
          target_(iter)
    {}

    NodeRef parentNode() const
    {
        assert(!at_end());
        return *parent_;
    }

    NodeRef childNode() const
    {
        assert(!at_end_of_level());
        return target_->second;
    }

    void advanceDepthFirst()
    {
        if (target_ != parent_->children_.end())
        {
            if (!target_->second.is_leaf())
            {
                auto& child = target_->second;
                parent_ = &child;
                target_ = child.children_.begin();
            }
            else
            {
                ++target_;
            }
        }
        else if (!parent_->is_sentinel())
        {
            target_ = parent_->position_;
            parent_ = parent_->parent_;
            if (!parent_->is_sentinel())
                ++target_;
            else
                target_ = parent_->children_.end();
        }
    }

    node_pointer parent_ = nullptr;
    iterator target_ = {};

    template <typename, bool> friend class TokenTrieCursor;

    template <typename, typename, typename, typename>
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
    return (lhs.parent_ == rhs.parent_) && (lhs.target_ == rhs.target_);
}

template <typename N, bool L, bool R>
bool operator!=(const TokenTrieCursor<N, L>& lhs,
                const TokenTrieCursor<N, R>& rhs)
{
    if (lhs.parent_ == nullptr || rhs.parent_ == nullptr)
        return lhs.parent_ != rhs.parent_;
    return (lhs.parent_ != rhs.parent_) || (lhs.target_ != rhs.target_);
}

} // namespace utils

} // namespace wamp


namespace std
{

template <typename K, typename T, typename C, typename A, typename Alloc>
struct uses_allocator<wamp::utils::TokenTrieNode<K,T,C,A>, Alloc> :
    std::is_convertible<Alloc, A>
{};

} // namespace std

#endif // CPPWAMP_UTILS_TOKENTRIENODE_HPP
