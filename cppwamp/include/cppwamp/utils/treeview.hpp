/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_UTILS_TREEVIEW_HPP
#define CPPWAMP_UTILS_TREEVIEW_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains the TreeView wrapper class. */
//------------------------------------------------------------------------------

#include <utility>
#include "../traits.hpp"

namespace wamp
{

namespace utils
{

//------------------------------------------------------------------------------
/** Wrapper around a general tree, which does not permit modification of the
    tree structure, but allows modification of mapped values if IsMutable is
    true.

    The view must be bound to a target tree to use any of its operations, except
    for the following which do not require a bound tree:
    - TreeView::operator bool
    - TreeView::empty
    - TreeView::size
    - TreeView::swap
    - TreeView::count
    - TreeView::contains
    - Non-member swap
    - Comparison operators

    @tparam TTree The type of the general tree being wrapped.
    @tparam IsMutable Allows modidication of mapped values if true. */
//------------------------------------------------------------------------------
template <typename TTree, bool IsMutable>
class TreeView
{
public:
    /// Type of tree being wrapped.
    using tree_type = TTree;

    /// Pointer to a tree to be wrapped.
    using tree_pointer = Conditional<IsMutable, tree_type*, const tree_type*>;

    /// Type used as a key within the tree.
    using key_type = typename tree_type::key_type;

    /// Type of the mapped value.
    using mapped_type = typename tree_type::mapped_type;

    /// Type used to pair a key and its associated mapped type.
    using value_type = typename tree_type::value_type;

    /// Type used to count the number of children in the tree.
    using size_type = typename tree_type::size_type;

    /// Type used to identify distance between iterators.
    using difference_type = typename tree_type::difference_type;

    /// Comparison function that determines how keys are sorted.
    using key_compare = typename tree_type::key_compare;

    /// Reference to a key-value pair.
    using reference = Conditional<IsMutable, value_type&, const value_type&>;

    /// Reference to an immutable key-value pair.
    using const_reference = const value_type&;

    /// Pointer to key-value pair
    using pointer = Conditional<IsMutable, typename tree_type::pointer,
                                typename tree_type::const_pointer>;

    /// Pointer to an immutable key-value pair
    using const_pointer = typename tree_type::const_pointer;

    /** Tree iterator type. */
    using iterator = Conditional<IsMutable, typename tree_type::iterator,
                                 typename tree_type::const_iterator>;

    /** Immutable tree iterator type. */
    using const_iterator = typename tree_type::const_iterator;

    /** Pair of iterators corresponding to a range. */
    using range_type = std::pair<iterator, iterator>;

    /** Pair of immutable iterators corresponding to a range. */
    using const_range_type = std::pair<const_iterator, const_iterator>;

    /** Function object type used for sorting key-value pairs. */
    using value_compare = typename tree_type::value_compare;

    /** Default-constructs a view that doesn't target any tree. */
    TreeView() = default;

    /** Constructs a view that targets the given tree. */
    explicit TreeView(tree_pointer tree) : tree_(tree) {}

    /** Converts from mutable to immutable tree view. */
    template <bool M, CPPWAMP_NEEDS(!IsMutable && M) = 0>
    TreeView(TreeView<TTree, M> rhs) // NOLINT(google-explicit-constructor)
        : tree_(rhs.tree_) {}

    /** Obtains the tree's allocator. */
    typename tree_type::allocator_type get_allocator() const noexcept
        {return tree().get_allocator();}

    /** Returns true if the tree view has a target. */
    explicit operator bool() const noexcept {return tree_ != nullptr;}

    /// @name Element Access
    /// @{

    /** Accesses the element associated with the given key, with
        bounds checking.
        @throws std::out_of_range if the tree does not have a child with the
                given key. */
    template <typename K>
    mapped_type& at(K&& key) {return tree().at(std::forward<K>(key));}

    /** Accesses the element associated with the given key, with
        bounds checking.
        @throws std::out_of_range if the tree does not have a child with the
                given key. */
    template <typename K>
    const mapped_type& at(key_type&& key) const
        {return tree().at(std::forward<K>(key));}

    /// @name Iterators
    /// @{

    /** Obtains an iterator to the beginning. */
    iterator begin() noexcept {return tree().begin();}

    /** Obtains an iterator to the beginning. */
    const_iterator begin() const noexcept {return cbegin();}

    /** Obtains an iterator to the beginning. */
    const_iterator cbegin() const noexcept {return tree().cbegin();}

    /** Obtains an iterator to the end. */
    iterator end() noexcept {return tree().end();}

    /** Obtains an iterator to the end. */
    const_iterator end() const noexcept {return cend();}

    /** Obtains an iterator to the end. */
    const_iterator cend() const noexcept {return tree().cend();}
    /// @}

    /// @name Capacity
    /// @{

    /** Checks whether the tree is empty. */
    bool empty() const noexcept {return tree_ ? tree_->empty() : true;}

    /** Obtains the number of children. */
    size_type size() const noexcept {return tree_ ? tree_->size() : 0;}

    /** Obtains the maximum possible number of children. */
    size_type max_size() const noexcept {return tree().max_size();}
    /// @}


    /// @name Modifiers

    /** Swaps the target of this view with the one from the given view. */
    void swap(TreeView& other) noexcept {std::swap(tree_, other.tree_);}
    /// @}

    /// @name Lookup
    /// @{

    /** Returns the number of children associated with the given key. */
    template <typename K>
    size_type count(K&& key) const
        {return tree_ ? tree_->count(std::forward<K>(key)) : 0;}

    /** Finds the child associated with the given key. */
    template <typename K>
    iterator find(K&& key) {return tree().find(std::forward<K>(key));}

    /** Finds the child associated with the given key. */
    template <typename K>
    const_iterator find(K&& key) const
        {return tree().find(std::forward<K>(key));}

    /** Checks if the tree contains the child with the given key. */
    template <typename K>
    bool contains(K&& key) const
        {return tree_ ? tree_->contains(std::forward<K>(key)) : false;}

    /** Obtains the range of children matching the given key.*/
    template <typename K>
    range_type equal_range(K&& key)
        {return tree().equal_range(std::forward<K>(key));}

    /** Obtains the range of children matching the given key.*/
    template <typename K>
    const_range_type equal_range(K&& key) const
        {return tree().equal_range(std::forward<K>(key));}

    /** Obtains an iterator to the first child not less than the given key. */
    template <typename K>
    iterator lower_bound(K&& key)
        {return tree().lower_bound(std::forward<K>(key));}

    /** Obtains an iterator to the first child not less than the given key. */
    template <typename K>
    const_iterator lower_bound(K&& key) const
        {return tree().lower_bound(std::forward<K>(key));}

    /** Obtains an iterator to the first child greater than than the
        given key. */
    template <typename K>
    iterator upper_bound(K&& key)
        {return tree().upperBound(std::forward<K>(key));}

    /** Obtains an iterator to the first child greater than than the
        given key. */
    template <typename K>
    const_iterator upper_bound(K&& key) const
        {return tree().upperBound(std::forward<K>(key));}

    /** Obtains the function that compares keys. */
    key_compare key_comp() const {return tree().key_comp();}

    /** Obtains the function that compares keys in value_type objects. */
    value_compare value_comp() const {return tree().value_comp();}
    /// @}

    /** Equality comparison. */
    friend bool operator==(TreeView a, TreeView b)
        {return !a ? !b : !!b && (a.tree_ == b.tree_);}

    /** Inequality comparison. */
    friend bool operator!=(TreeView a, TreeView b)
        {return !a ? !b : !b || (a.tree_ != b.tree_);}

    /** Less-than comparison. */
    friend bool operator<(TreeView a, TreeView b)
        {return !a ? !!b : !!b && (a.tree_ < b.tree_);}

    /** Less-than-or-equal comparison. */
    friend bool operator<=(TreeView a, TreeView b)
        {return !a ? true : !!b && (a.tree_ <= b.tree_);}

    /** Greater-than comparison. */
    friend bool operator>(TreeView a, TreeView b)
        {return !a ? false : !b || (a.tree_ > b.tree_);}

    /** Greater-than-or-equal comparison. */
    friend bool operator>=(TreeView a, TreeView b)
        {return !a ? !b : !b || (a.tree_ >= b.tree_);}

    /** Non-member swap. */
    friend void swap(TreeView& a, TreeView& b) noexcept {a.swap(b);}

private:
    using TreeRef = Conditional<IsMutable, tree_type&, const tree_type&>;

    TreeRef tree() {assert(tree_ != nullptr); return *tree_;}

    const tree_type& tree() const {assert(tree_ != nullptr); return *tree_;}

    tree_pointer tree_ = nullptr;

    template <typename, bool> friend class TreeView;
};

} // namespace utils

} // namespace wamp

#endif // CPPWAMP_UTILS_TREEVIEW_HPP
