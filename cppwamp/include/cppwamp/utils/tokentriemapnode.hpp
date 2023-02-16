/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_UTILS_TOKENTRIEMAPNODE_HPP
#define CPPWAMP_UTILS_TOKENTRIEMAPNODE_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains the TokenTrieMap node and cursor facilities. */
//------------------------------------------------------------------------------

#include <algorithm>
#include <cassert>
#include <initializer_list>
#include <map>
#include <utility>

#include "../tagtypes.hpp"
#include "../traits.hpp"
#include "treeview.hpp"

namespace wamp
{

namespace utils
{

namespace internal
{
    template <typename, typename, typename, typename> class TokenTrieMapImpl;
}

//------------------------------------------------------------------------------
template <typename K, typename T, typename C, typename A>
class TokenTrieMapNode
{
private:
    struct PassKey {};

    using AllocTraits =
        typename std::allocator_traits<A>::template rebind_traits<
            std::pair<const K, T>>;

public:
    /** Split token container type used as the key. */
    using key_type = K;

    /** Type of the mapped value. */
    using mapped_type = T;

    /// Comparison function that determines how keys are sorted.
    using key_compare = C;

    /// Token type associated with a node.
    using token_type = typename key_type::value_type;

    /// Key-value pair type for element.
    using element_type = std::pair<const key_type, mapped_type>;

    /// Allocator type passed to the TokenTrieMap using this node.
    using allocator_type =
        typename std::allocator_traits<A>::template rebind_alloc<element_type>;

    /// Pointer to a key-value pair.
    using element_pointer = typename AllocTraits::pointer;

    /// Pointer to an immutable key-value pair.
    using const_element_pointer = typename AllocTraits::const_pointer;

    /// Allocator type used by the tree contained by this node.
    using tree_allocator_type =
        typename std::allocator_traits<A>::template rebind_alloc<
            std::pair<const token_type, TokenTrieMapNode>>;

    /// Tree type contained by this node.
    using tree_type = std::map<token_type, TokenTrieMapNode, key_compare,
                               tree_allocator_type>;

    /** Wrapper around the contained tree that prevents modifying its structure,
        while allowing its mapped values to be modified. */
    using tree_view_type = TreeView<tree_type, true>;

    /** Wrapper around the contained tree that prevents modifying its structure
        and its mapped values. */
    using const_tree_view_type = TreeView<tree_type, false>;

    /** Copy constructor. */
    TokenTrieMapNode(const TokenTrieMapNode& other)
        : children_(other.children_)
    {
        if (other.has_element())
            constructElement(other.value());
    }

    /** Copy constructor taking an allocator. */
    TokenTrieMapNode(std::allocator_arg_t, const tree_allocator_type& alloc,
                     const TokenTrieMapNode& other)
        : children_(other.children_, alloc)
    {
        if (other.has_element())
            constructElement(other.value());
    }

    /** Non-move-constructible. */
    TokenTrieMapNode(TokenTrieMapNode&& other) = delete;

    /** Non-copy-assignable. */
    TokenTrieMapNode& operator=(const TokenTrieMapNode&) = delete;

    /** Non-move-assignable. */
    TokenTrieMapNode& operator=(TokenTrieMapNode&&) = delete;

    /** Destructor. */
    ~TokenTrieMapNode() {destroyElement();}

    /** Determines if this is the sentinel node. */
    bool is_sentinel() const noexcept {return parent_ == nullptr;}

    /** Determines if this is the root node. */
    bool is_root() const noexcept
        {return !is_sentinel() && parent_->is_sentinel();}

    /** Returns true is this node has no children. */
    bool is_leaf() const noexcept {return children_.empty();}

    /** Determines if this node has a mapped value. */
    bool has_element() const noexcept {return bool(element_);}

    /** Obtains a pointer to the node's parent, or `nullptr` if this is
        the sentinel node. */
    TokenTrieMapNode* parent() noexcept {return parent_;}

    /** Obtains a pointer to the node's parent, or `nullptr` if this is
        the sentinel node. */
    const TokenTrieMapNode* parent() const noexcept {return parent_;}

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

    /** Accesses the element associated with this node.
        @pre `this->has_element()` */
    element_type& element() {assert(has_element()); return *element_;}

    /** Accesses the value associated with this node.
        @pre `this->has_element()` */
    const element_type& element() const
        {assert(has_element()); return *element_;}

    /** Accesses the split token key associated with this node.
        @pre `this->has_element` */
    const key_type& key() const {return element().first;}

    /** Accesses the value associated with this node.
        @pre `this->has_element()` */
    mapped_type& value() {return element().second;}

    /** Accesses the value associated with this node.
        @pre `this->has_element()` */
    const mapped_type& value() const {return element().second;}

    /** Obtains a view of the node's child tree.
        @pre `!this->is_sentinel` */
    tree_view_type children() {assert(!is_sentinel()); return {&children_};}

    /** Obtains a view of the node's child tree.
        @pre `!this->is_sentinel` */
    const_tree_view_type children() const
        {assert(!is_sentinel()); return {&children_};}

private:
    using TreeIterator = typename tree_type::iterator;

    template <typename... Us>
    void setElement(key_type&& key, Us&&... args)
    {
        if (has_element())
            value() = mapped_type(std::forward<Us>(args)...);
        else
            constructElementWithKey(std::move(key), std::forward<Us>(args)...);
    }

    void clearValue()
    {
        if (has_element())
            destroyElement();
    }

    template <typename... Us>
    void constructElement(Us&&... args)
    {
        assert(element_ == nullptr);
        allocator_type alloc{children_.get_allocator()};
        element_pointer ptr = {};
        try
        {
            ptr = AllocTraits::allocate(alloc, sizeof(element_type));
            AllocTraits::construct(
                alloc, ptr, std::piecewise_construct,
                std::forward_as_tuple(),
                std::forward_as_tuple(std::forward<Us>(args)...));
        }
        catch (...)
        {
            if (ptr)
                AllocTraits::deallocate(alloc, ptr, sizeof(element_type));
            throw;
        }
        element_ = ptr;
    }

    template <typename... Us>
    void constructElementWithKey(key_type&& key, Us&&... args)
    {
        assert(element_ == nullptr);
        allocator_type alloc{children_.get_allocator()};
        element_pointer ptr = {};
        try
        {
            ptr = AllocTraits::allocate(alloc, sizeof(element_type));
            AllocTraits::construct(
                alloc, ptr, std::piecewise_construct,
                std::forward_as_tuple(std::move(key)),
                std::forward_as_tuple(std::forward<Us>(args)...));
        }
        catch (...)
        {
            if (ptr)
                AllocTraits::deallocate(alloc, ptr, sizeof(element_type));
            throw;
        }
        element_ = ptr;
    }

    void destroyElement()
    {
        if (!has_element())
            return;

        allocator_type alloc{children_.get_allocator()};
        AllocTraits::destroy(alloc, element_);
        AllocTraits::deallocate(alloc, element_, sizeof(element_type));
        element_ = {};
    }

    tree_type children_;
    TreeIterator position_ = {};
    element_pointer element_ = {};
    TokenTrieMapNode* parent_ = nullptr;

    template <typename, bool> friend class TokenTrieMapCursor;

    template <typename, bool> friend class TokenTrieMapIterator;

    template <typename, typename, typename, typename>
    friend class internal::TokenTrieMapImpl;

public: // Internal use only
    TokenTrieMapNode(PassKey) : position_(children_.end()) {}

    TokenTrieMapNode(PassKey, key_compare comp)
        : children_(std::move(comp)),
          position_(children_.end())
    {}

    TokenTrieMapNode(std::allocator_arg_t, const tree_allocator_type& alloc,
                     PassKey, key_compare comp)
        : children_(std::move(comp), alloc),
          position_(children_.end())
    {}

    template <typename... Us>
    TokenTrieMapNode(PassKey, key_compare comp, in_place_t, Us&&... args)
        : children_(std::move(comp))
    {
        constructElement(std::forward<Us>(args)...);
    }

    template <typename... Us>
    TokenTrieMapNode(std::allocator_arg_t, const tree_allocator_type& alloc,
                     PassKey, key_compare comp, in_place_t, Us&&... args)
        : children_(std::move(comp), alloc)
    {
        constructElement(std::forward<Us>(args)...);
    }

    template <typename... Us>
    TokenTrieMapNode(PassKey, key_compare comp, key_type&& key,
                     in_place_t, Us&&... args)
        : children_(std::move(comp))
    {
        constructElementWithKey(std::move(key), std::forward<Us>(args)...);
    }

    template <typename... Us>
    TokenTrieMapNode(std::allocator_arg_t, const tree_allocator_type& alloc,
                     PassKey, key_compare comp, key_type&& key,
                     in_place_t, Us&&... args)
        : children_(std::move(comp), alloc)
    {
        constructElementWithKey(std::move(key), std::forward<Us>(args)...);
    }

    TokenTrieMapNode(PassKey, TokenTrieMapNode&& other)
        : children_(std::move(other.children_)),
          position_(std::move(other.position_)),
          element_(other.element_),
          parent_(other.parent_)
    {
        other.element_ = nullptr;
    }

    TokenTrieMapNode(std::allocator_arg_t, const tree_allocator_type& alloc,
                     PassKey, TokenTrieMapNode&& other)
        : children_(std::move(other.children_)),
          position_(std::move(other.position_)),
          element_(other.element_),
          parent_(other.parent_)
    {
        other.element_ = nullptr;
    }
};

//------------------------------------------------------------------------------
/** Type used to traverse nodes in a TokenTrieMap.
    This type is intended for trie algorithms where a forward iterator that only
    traverses value nodes is insuffient.
    @tparam N The node type being traversed.
    @tparam IsMutable Allows node values to be modified when true. */
//------------------------------------------------------------------------------
template <typename N, bool IsMutable>
class TokenTrieMapCursor
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

    /// Key-value pair type stored in the node.
    using element_type = typename N::element_type;

    /// Type of the mapped value.
    using mapped_type = typename N::mapped_type;

    /** Wrapper around a node tree that prevents modifying its structure. */
    using tree_view_type = TreeView<typename N::tree_type, IsMutable>;

    /** Wrapper around a node tree that prevents modifying its structure. */
    using const_tree_view_type = TreeView<typename N::tree_type, false>;

    /** Reference to a mapped value. */
    using reference = Conditional<IsMutable, mapped_type&, const mapped_type&>;

    /** Reference to a key-value pair. */
    using element_reference = Conditional<IsMutable, element_type&,
                                          const element_type&>;

    /** Iterator type which advances through a tree's child nodes in a
        breath-first manner (non-recursive). */
    using const_iterator = typename node_type::tree_type::const_iterator;

    /** Iterator type which advances through a tree's child nodes in a
        breath-first manner (non-recursive). */
    using iterator = Conditional<IsMutable,
                                 typename node_type::tree_type::iterator,
                                 const_iterator>;
    using node_pointer = Conditional<IsMutable, node_type*, const node_type*>;

    /** True if this cursor allows mapped values to be modified. */
    static constexpr bool is_mutable() {return IsMutable;}

    /** Default constructs a cursor that does not point to any node. */
    TokenTrieMapCursor() = default;

    /** Conversion from mutable cursor to const cursor. */
    template <bool RM, CPPWAMP_NEEDS(!IsMutable && RM) = 0>
    TokenTrieMapCursor(const TokenTrieMapCursor<N, RM>& rhs)
        : parent_(rhs.parent_),
          target_(rhs.target_)
    {}

    /** Assignment from mutable cursor to const cursor. */
    template <bool RM, CPPWAMP_NEEDS(!IsMutable && RM) = 0>
    TokenTrieMapCursor& operator=(const TokenTrieMapCursor<N, RM>& rhs)
    {
        parent_ = rhs.parent_;
        target_ = rhs.target_;
        return *this;
    }

    /** Same as TokenTrieMapCursor::good */
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
    bool has_element() const noexcept
        {return !at_end_of_level() && childNode().has_element();}

    /** Determines if the token and mapped value of this cursor's node are
        equivalent to the ones from the given cursor.
        If either cursor is not good, they are considered equivalent if and
        only if both cursors are not good. */
    bool token_and_value_equals(const TokenTrieMapCursor& rhs) const
    {
        if (!good())
            return !rhs.good();
        if (!rhs.good() || tokensAreNotEquivalent(token(), rhs.token()))
            return false;

        const auto& a = childNode();
        const auto& b = rhs.childNode();
        return a.has_element() ? (b.has_element() && (a.value() == b.value()))
                             : !b.has_element();
    }

    /** Determines if the token or mapped value of this cursor's node are
        different to the ones from the given cursor.
        If either cursor is not good, they are considered different if and
        only if the cursors are not both bad. */
    bool token_or_value_differs(const TokenTrieMapCursor& rhs) const
    {
        if (!good())
            return rhs.good();
        if (!rhs.good() || tokensAreNotEquivalent(token(), rhs.token()))
            return true;

        const auto& a = childNode();
        const auto& b = rhs.childNode();
        return a.has_element() ? (!b.has_element() || (a.value() != b.value()))
                               : b.has_element();
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

    /** Obtains the token associated with the current target node.
        @pre `!this->at_end_of_level() */
    const token_type& token() const
        {assert(!at_end_of_level()); return target_->first;}

    /** Accesses the element associated with the current target node.
    @pre `this->has_element()` */
    const element_type& element() const
        {assert(has_element()); return target_->second.element();}

    /** Accesses the mapped value associated with the current target node.
    @pre `this->has_element()` */
    element_reference element()
        {assert(has_element()); return target_->second.element();}

    /** Accesses the key associated with the current target node.
        @pre `this->has_element()` */
    const key_type& key() const {return element().first;}

    /** Accesses the mapped value associated with the current target node.
        @pre `this->has_element()` */
    const mapped_type& value() const {return element().second;}

    /** Accesses the mapped value associated with the current target node.
        @pre `this->has_element()` */
    reference value() {return element().second;}

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
            if (has_element())
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
    using NodeRef = Conditional<IsMutable, node_type&, const node_type&>;
    using KeyComp = typename node_type::key_compare;

    static TokenTrieMapCursor begin(NodeRef rootNode)
    {
        return TokenTrieMapCursor(&rootNode, rootNode.children_.begin());
    }

    static TokenTrieMapCursor first(NodeRef rootNode)
    {
        auto cursor = begin(rootNode);
        if (!cursor.at_end_of_level() && !cursor.target()->has_element())
            cursor.advance_depth_first_to_next_element();
        return cursor;
    }

    static TokenTrieMapCursor end(NodeRef sentinelNode)
    {
        return TokenTrieMapCursor(&sentinelNode, sentinelNode.children_.end());
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

    TokenTrieMapCursor(node_pointer node, iterator iter)
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

    template <typename, bool> friend class TokenTrieMapCursor;

    template <typename, bool> friend class TokenTrieMapIterator;

    template <typename, typename, typename, typename>
    friend class internal::TokenTrieMapImpl;

    template <typename TNode, bool L, bool R>
    friend bool operator==(const TokenTrieMapCursor<TNode, L>& lhs,
                           const TokenTrieMapCursor<TNode, R>& rhs);

    template <typename TNode, bool L, bool R>
    friend bool operator!=(const TokenTrieMapCursor<TNode, L>& lhs,
                           const TokenTrieMapCursor<TNode, R>& rhs);
};

template <typename N, bool L, bool R>
bool operator==(const TokenTrieMapCursor<N, L>& lhs,
                const TokenTrieMapCursor<N, R>& rhs)
{
    if (lhs.parent_ == nullptr || rhs.parent_ == nullptr)
        return lhs.parent_ == rhs.parent_;
    return (lhs.parent_ == rhs.parent_) && (lhs.target_ == rhs.target_);
}

template <typename N, bool L, bool R>
bool operator!=(const TokenTrieMapCursor<N, L>& lhs,
                const TokenTrieMapCursor<N, R>& rhs)
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
struct uses_allocator<wamp::utils::TokenTrieMapNode<K,T,C,A>, Alloc> :
    std::is_convertible<Alloc, A>
{};

} // namespace std

#endif // CPPWAMP_UTILS_TOKENTRIEMAPNODE_HPP
